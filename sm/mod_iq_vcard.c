/*
 * jabberd - Jabber Open Source Server
 * Copyright (c) 2002 Jeremie Miller, Thomas Muldowney,
 *                    Ryan Eatmon, Robert Norris
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA
 */

#include "sm.h"

/** @file sm/mod_iq_vcard.c
  * @brief user profiles (vcard)
  * @author Robert Norris
  * $Date: 2005/08/17 07:48:28 $
  * $Revision: 1.25 $
  */

#define uri_VCARD    "vcard-temp"
static int ns_VCARD = 0;

#define VCARD_MAX_FIELD_SIZE    (16384)

/**
 * these are the vcard attributes that gabber supports. they're also
 * all strings, and thus easy to automate. there might be more in
 * regular use, we need to check that out. one day, when we're all
 * using real foaf profiles, we'll have bigger things to worry about :)
 *
 * darco(2005-09-15): Added quite a few more fields, including those
 * necessary for vCard avatar support. 
 */

static char *_iq_vcard_map[] = {
    "FN",           "fn",
    "NICKNAME",     "nickname",
    "URL",          "url",
    "TEL/NUMBER",   "tel",
    "EMAIL/USERID", "email",
    "TITLE",        "title",
    "ROLE",         "role",
    "BDAY",         "bday",
    "DESC",         "desc",
    "N/FAMILY",     "n-family",
    "N/GIVEN",      "n-given",
    "N/MIDDLE",     "n-middle",
    "N/PREFIX",     "n-prefix",
    "N/SUFFIX",     "n-suffix",
    "ADR/STREET",   "adr-street",
    "ADR/POBOX",    "adr-pobox",
    "ADR/EXTADD",   "adr-extadd",
    "ADR/LOCALITY", "adr-locality",
    "ADR/REGION",   "adr-region",
    "ADR/PCODE",    "adr-pcode",
    "ADR/CTRY",     "adr-country",
    "ORG/ORGNAME",  "org-orgname",
    "ORG/ORGUNIT",  "org-orgunit",
    
    "TZ",           "tz",
    "GEO/LAT",      "geo-lat",
    "GEO/LON",      "geo-lon",
    "AGENT/EXTVAL", "agent-extval",
    "NOTE",         "note",
    "REV",          "rev",
    "SORT-STRING",  "sort-string",

    "KEY/TYPE",     "key-type",
    "KEY/CRED",     "key-cred",
    
    "PHOTO/TYPE",   "photo-type",
    "PHOTO/BINVAL", "photo-binval",
    "PHOTO/EXTVAL", "photo-extval",
    
    "LOGO/TYPE",    "logo-type",
    "LOGO/BINVAL",  "logo-binval",
    "LOGO/EXTVAL",  "logo-extval",
    
    "SOUND/PHONETIC","sound-phonetic",
    "SOUND/BINVAL", "sound-binval",
    "SOUND/EXTVAL", "sound-extval",
    
    NULL,           NULL
};

static os_t _iq_vcard_to_object(pkt_t pkt) {
    os_t os;
    os_object_t o;
    int i = 0, elem;
    char *vkey, *dkey, *vskey, ekey[10], cdata[VCARD_MAX_FIELD_SIZE];

    log_debug(ZONE, "building object from packet");

    os = os_new();
    o = os_object_new(os);
    
    while(_iq_vcard_map[i] != NULL) {
        vkey = _iq_vcard_map[i];
        dkey = _iq_vcard_map[i + 1];

        i += 2;

        vskey = strchr(vkey, '/');
        if(vskey == NULL) {
            vskey = vkey;
            elem = 2;
        } else {
            sprintf(ekey, "%.*s", vskey - vkey, vkey);
            elem = nad_find_elem(pkt->nad, 2, NAD_ENS(pkt->nad, 2), ekey, 1);
            if(elem < 0)
                continue;
            vskey++;
        }

        elem = nad_find_elem(pkt->nad, elem, NAD_ENS(pkt->nad, 2), vskey, 1);
        if(elem < 0 || NAD_CDATA_L(pkt->nad, elem) == 0)
            continue;

        log_debug(ZONE, "extracted vcard key %s val '%.*s' for db key %s", vkey, NAD_CDATA_L(pkt->nad, elem), NAD_CDATA(pkt->nad, elem), dkey);

        snprintf(cdata, sizeof(cdata), "%.*s", NAD_CDATA_L(pkt->nad, elem), NAD_CDATA(pkt->nad, elem));
        cdata[sizeof(cdata)-1] = '\0';
        os_object_put(o, dkey, cdata, os_type_STRING);
    }

    return os;
}

static pkt_t _iq_vcard_to_pkt(sm_t sm, os_t os) {
    pkt_t pkt;
    os_object_t o;
    int i = 0, elem;
    char *vkey, *dkey, *vskey, ekey[10], *dval;
    
    log_debug(ZONE, "building packet from object");

    pkt = pkt_create(sm, "iq", "result", NULL, NULL);
    nad_append_elem(pkt->nad, nad_add_namespace(pkt->nad, uri_VCARD, NULL), "vCard", 2);

    if(!os_iter_first(os))
        return pkt;
    o = os_iter_object(os);

    while(_iq_vcard_map[i] != NULL) {
        vkey = _iq_vcard_map[i];
        dkey = _iq_vcard_map[i + 1];

        i += 2;

        if(!os_object_get_str(os, o, dkey, &dval))
            continue;

        vskey = strchr(vkey, '/');
        if(vskey == NULL) {
            vskey = vkey;
            elem = 2;
        } else {
            sprintf(ekey, "%.*s", vskey - vkey, vkey);
            elem = nad_find_elem(pkt->nad, 2, NAD_ENS(pkt->nad, 2), ekey, 1);
            if(elem < 0)
                elem = nad_append_elem(pkt->nad, NAD_ENS(pkt->nad, 2), ekey, 3);
            vskey++;
        }

        log_debug(ZONE, "extracted dbkey %s val '%s' for vcard key %s", dkey, dval, vkey);

        nad_append_elem(pkt->nad, NAD_ENS(pkt->nad, 2), vskey, pkt->nad->elems[elem].depth + 1);
        nad_append_cdata(pkt->nad, dval, strlen(dval), pkt->nad->elems[elem].depth + 2);
    }

    return pkt;
}

static mod_ret_t _iq_vcard_in_sess(mod_instance_t mi, sess_t sess, pkt_t pkt) {
    os_t os;
    st_ret_t ret;
    pkt_t result;

    /* only handle vcard sets and gets that aren't to anyone */
    if(pkt->to != NULL || (pkt->type != pkt_IQ && pkt->type != pkt_IQ_SET) || pkt->ns != ns_VCARD)
        return mod_PASS;

    /* get */
    if(pkt->type == pkt_IQ) {
        ret = storage_get(sess->user->sm->st, "vcard", jid_user(sess->jid), NULL, &os);
        switch(ret) {
            case st_FAILED:
                return -stanza_err_INTERNAL_SERVER_ERROR;

            case st_NOTIMPL:
                return -stanza_err_FEATURE_NOT_IMPLEMENTED;

            case st_NOTFOUND:
                nad_set_attr(pkt->nad, 1, -1, "type", "result", 6);
                nad_set_attr(pkt->nad, 1, -1, "to", NULL, 0);
                nad_set_attr(pkt->nad, 1, -1, "from", NULL, 0);

                pkt_sess(pkt, sess);

                return mod_HANDLED;

            case st_SUCCESS:
                result = _iq_vcard_to_pkt(sess->user->sm, os);
                os_free(os);

                nad_set_attr(result->nad, 1, -1, "type", "result", 6);
                pkt_id(pkt, result);

                pkt_sess(result, sess);

                pkt_free(pkt);

                return mod_HANDLED;
        }

        /* we never get here */
        pkt_free(pkt);
        return mod_HANDLED;
    }

    os = _iq_vcard_to_object(pkt);
    ret = storage_replace(sess->user->sm->st, "vcard", jid_user(sess->jid), NULL, os);
    os_free(os);

    switch(ret) {
        case st_FAILED:
            return -stanza_err_INTERNAL_SERVER_ERROR;

        case st_NOTIMPL:
            return -stanza_err_FEATURE_NOT_IMPLEMENTED;

        default:
            result = pkt_create(sess->user->sm, "iq", "result", NULL, NULL);

            pkt_id(pkt, result);

            pkt_sess(result, sess);
            
            pkt_free(pkt);

            return mod_HANDLED;
    }

    /* we never get here */
    pkt_free(pkt);
    return mod_HANDLED;
}

static mod_ret_t _iq_vcard_pkt_user(mod_instance_t mi, user_t user, pkt_t pkt) {
    os_t os;
    st_ret_t ret;
    pkt_t result;

    /* only handle vcard sets and gets */
    if((pkt->type != pkt_IQ && pkt->type != pkt_IQ_SET) || pkt->ns != ns_VCARD)
        return mod_PASS;

    /* error them if they're trying to do a set */
    if(pkt->type == pkt_IQ_SET)
        return -stanza_err_FORBIDDEN;

    ret = storage_get(user->sm->st, "vcard", jid_user(user->jid), NULL, &os);
    switch(ret) {
        case st_FAILED:
            return -stanza_err_INTERNAL_SERVER_ERROR;

        case st_NOTIMPL:
            return -stanza_err_FEATURE_NOT_IMPLEMENTED;

        case st_NOTFOUND:
            return -stanza_err_ITEM_NOT_FOUND;

        case st_SUCCESS:
            result = _iq_vcard_to_pkt(user->sm, os);
            os_free(os);

            result->to = jid_dup(pkt->from);
            result->from = jid_dup(pkt->to);

            nad_set_attr(result->nad, 1, -1, "to", jid_full(result->to), 0);
            nad_set_attr(result->nad, 1, -1, "from", jid_full(result->from), 0);

            pkt_id(pkt, result);

            pkt_router(result);

            pkt_free(pkt);

            return mod_HANDLED;
    }

    /* we never get here */
    pkt_free(pkt);
    return mod_HANDLED;
}

static void _iq_vcard_user_delete(mod_instance_t mi, jid_t jid) {
    log_debug(ZONE, "deleting vcard for %s", jid_user(jid));

    storage_delete(mi->sm->st, "vcard", jid_user(jid), NULL);
}

static void _iq_vcard_free(module_t mod) {
    sm_unregister_ns(mod->mm->sm, uri_VCARD);
    feature_unregister(mod->mm->sm, uri_VCARD);
}

DLLEXPORT int module_init(mod_instance_t mi, char *arg) {
    module_t mod = mi->mod;

    if(mod->init) return 0;

    mod->in_sess = _iq_vcard_in_sess;
    mod->pkt_user = _iq_vcard_pkt_user;
    mod->user_delete = _iq_vcard_user_delete;
    mod->free = _iq_vcard_free;

    ns_VCARD = sm_register_ns(mod->mm->sm, uri_VCARD);
    feature_register(mod->mm->sm, uri_VCARD);

    return 0;
}
