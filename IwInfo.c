#include <unistd.h>
#include <stdlib.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>
#include <errno.h>

/* copyed from iw */
#include "nl80211.h"

#include "jni.h"
#include <nativehelper/JNIHelp.h>

#define NATIVE_METHOD(className, functionName, signature) \
{ #functionName, signature, (void*)(fengmi_net_ ## className ## _ ## functionName) }

enum command_identify_by {
    CIB_NONE,
    CIB_PHY,
    CIB_NETDEV,
    CIB_WDEV,
};

struct nl_info {
    struct nl_sock *nl_sock;
    int nl80211_id;
    int he_support;
};

void parse_he_info(struct nlattr *nl_iftype, struct nl_info *nli)
{
    int i;
    struct nlattr *tb_iftype[NL80211_BAND_IFTYPE_ATTR_MAX + 1];
    struct nlattr *tb_iftype_flags[NL80211_IFTYPE_MAX + 1];

    nla_parse(tb_iftype, NL80211_BAND_IFTYPE_ATTR_MAX, nla_data(nl_iftype), nla_len(nl_iftype), NULL);

    if (!tb_iftype[NL80211_BAND_IFTYPE_ATTR_IFTYPES])
        return;

    if (nla_parse_nested(tb_iftype_flags, NL80211_IFTYPE_MAX, tb_iftype[NL80211_BAND_IFTYPE_ATTR_IFTYPES], NULL))
        return;

    for (i = 0; i < NUM_NL80211_IFTYPES; i++) {
        if (nla_get_flag(tb_iftype_flags[i])) {
            if (i == NL80211_IFTYPE_STATION) {
               nli->he_support = 1; 
            }
        }
    }

    return;
}

static int nlCallback(struct nl_msg* msg, void* arg)
{
    struct nlmsghdr* ret_hdr = nlmsg_hdr(msg);
    struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
    struct nl_info *nli = (struct nl_info *)arg;

    if (ret_hdr->nlmsg_type != nli->nl80211_id) {
        return NL_STOP;
    }

    struct genlmsghdr *gnlh = (struct genlmsghdr*) nlmsg_data(ret_hdr);

    nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
            genlmsg_attrlen(gnlh, 0), NULL);

    if (tb_msg[NL80211_ATTR_WIPHY_BANDS]) {
        int rem_band;
        struct nlattr *nl_band;
        struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];

        nla_for_each_nested(nl_band, tb_msg[NL80211_ATTR_WIPHY_BANDS], rem_band) {
            nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band), nla_len(nl_band), NULL);
            if (tb_band[NL80211_BAND_ATTR_IFTYPE_DATA]) {
                struct nlattr *nl_iftype;
                int rem_band_iftype;
                nla_for_each_nested(nl_iftype, tb_band[NL80211_BAND_ATTR_IFTYPE_DATA], rem_band_iftype) {
                    parse_he_info(nl_iftype, nli);
                }
            }
        }
    }

    return NL_SKIP;
}

static int phy_lookup(const char *name)
{
    char buf[200];
    int fd, pos;

    snprintf(buf, sizeof(buf), "/sys/class/ieee80211/%s/index", name);

    fd = open(buf, O_RDONLY);
    if (fd < 0)
        return -1;
    pos = read(fd, buf, sizeof(buf) - 1);
    if (pos < 0) {
        close(fd);
        return -1;
    }
    buf[pos] = '\0';
    close(fd);
    return atoi(buf);
}

static int nl80211_init(struct nl_info *nli)
{
    int err;

    nli->nl_sock = nl_socket_alloc();
    if (!nli->nl_sock) {
        return -ENOMEM;
    }

    nl_socket_set_buffer_size(nli->nl_sock, 8192, 8192);

    if (genl_connect(nli->nl_sock)) {
        err = -ENOLINK;
        goto out_handle_destroy;
    }

    nli->nl80211_id = genl_ctrl_resolve(nli->nl_sock, "nl80211");
    if (nli->nl80211_id < 0) {
        err = -ENOENT;
        goto out_handle_destroy;
    }

    return 0;

out_handle_destroy:
    nl_socket_free(nli->nl_sock);
    return err;
}

static void nl80211_cleanup(struct nl_info *nli)
{
    nl_socket_free(nli->nl_sock);
}

static int error_handler(struct sockaddr_nl *nla, struct nlmsgerr *err,
        void *arg)
{
    int *ret = arg;
    *ret = err->error;
    return NL_STOP;
}

static int finish_handler(struct nl_msg *msg, void *arg)
{
    int *ret = arg;
    *ret = 0;
    return NL_SKIP;
}

static int ack_handler(struct nl_msg *msg, void *arg)
{
    int *ret = arg;
    *ret = 0;
    return NL_STOP;
}

int run_nl80211(struct nl_info *nli, enum command_identify_by idby, const char *name)
{
    int err = -1;
    int devidx = -1;
    struct nl_cb *cb = NULL;
    struct nl_cb *s_cb = NULL;
    struct nl_msg *msg = NULL;

    msg = nlmsg_alloc();
    if (!msg) {
        return -1;
    }

    cb = nl_cb_alloc(NL_CB_DEFAULT);
    s_cb = nl_cb_alloc(NL_CB_DEFAULT);
    if (!cb || !s_cb) {
        goto out_free_msg;
    }

    genlmsg_put(msg, 0, 0, nli->nl80211_id, 0, 0, NL80211_CMD_GET_WIPHY, 0);

    switch (idby) {
        case CIB_PHY:
            devidx = phy_lookup(name);
            NLA_PUT_U32(msg, NL80211_ATTR_WIPHY, devidx);
            break;
        case CIB_NETDEV:
            devidx = if_nametoindex(name);
            NLA_PUT_U32(msg, NL80211_ATTR_IFINDEX, devidx);
            break;
        default:
            break;
    }

    nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, nlCallback, nli);
    nl_socket_set_cb(nli->nl_sock, s_cb);

    err = nl_send_auto_complete(nli->nl_sock, msg);
    if (err < 0) {
        goto out;
    }

    err = 1;

    nl_cb_err(cb, NL_CB_CUSTOM, error_handler, &err);
    nl_cb_set(cb, NL_CB_FINISH, NL_CB_CUSTOM, finish_handler, &err);
    nl_cb_set(cb, NL_CB_ACK, NL_CB_CUSTOM, ack_handler, &err);

    while (err > 0)
        nl_recvmsgs(nli->nl_sock, cb);

out:
    nl_cb_put(cb);
out_free_msg:
    nlmsg_free(msg);
    return err;
nla_put_failure:
    return -1;
}

int get_wiphy(enum command_identify_by idby, const char *name)
{
    struct nl_info nli = {0};

    if (nl80211_init(&nli)) {
        return -1;
    }

    if (run_nl80211(&nli, idby, name) < 0) {
        goto error;
    }

    nl80211_cleanup(&nli);

    printf("HE_SUPPORT:%s\n", nli.he_support ? "Supported" : "Unsupport");

    return !!(nli.he_support);

error:
    nl80211_cleanup(&nli);

    return -1;
}

JNIEXPORT jint JNICALL
fengmi_net_IwInfo_getHeSupport(JNIEnv *env, jclass this)
{
    return get_wiphy(CIB_PHY, "phy0");
}

JNINativeMethod gMethods[] = {
    NATIVE_METHOD(IwInfo, getHeSupport, "()I"),
};

void register_fengmi_net_iwinfo(JNIEnv* env)
{
    /* 这里注册的是调用者的类的path, 即:
     * com.android.commands.geth.Wifi
     * */
    jniRegisterNativeMethods(env, "com/android/commands/geth/Wifi", gMethods, NELEM(gMethods));
/*
    jclass clazz;
    clazz = env->FindClass("fengmi/net/IwInfo");
    if (clazz == NULL) {
        printf("FindClass failed.\n");
        return;
    }

    if (env->RegisterNatives(clazz, gMethods, sizeof(gMethods) / sizeof(gMethods[0])) < 0) {
        printf("RegisterNatives failed.\n");
        return;
    }
*/

  return;
}

