#include "../common/define.h"
#include "../cgi-util/cgi-util.h"
#include "../base64/base64.h"
#include "../common/bstr.h"
#include <libconfig.h>
#include <err.h>
#include "cgihandler.h"
#include "library.h"

void dispatcher_cgi_init(void) {
	int res = cgi_init();
	if (res != CGIERR_NONE)
		errx(1, "cgi init error %d: %s", res, cgi_strerror(res));
}

/**
 * Access combinations:
 * 	No secret key && valid user -> limited access
 *	secret key && valid key && in access list -> full access
 *	connection from localhost -> full access
 *
 * All other combinations: no access
 */
int cgi_access_type(struct config_t *cfg, char *remoteaddr) {
	if (strcmp(remoteaddr, "127.0.0.1") == 0)
		return ACCESS_TYPE_FULL;

	const char *secret_key = cgi_getentrystr("secret");

	/*extern char **environ;
	char **next = environ;
	while (*next) {
		warnx("environ %s", *next);
		next++;
	}*/

	// Limited access if no key, but user is logged in
	if (secret_key == NULL) {
		if (getenv("REDIRECT_REMOTE_USER") != NULL) {
			return ACCESS_TYPE_LIMITED;
		}
		
		warn("no key, no logged in user");
		return ACCESS_TYPE_NONE;
	}
	
	// Full access if host:key match in config.
	int num_hosts;
	config_setting_t *setting;
	setting = config_lookup(cfg, "access");
	if (setting == NULL || !(num_hosts = config_setting_length(setting))) {
		warnx("No hosts in dispatcher 'access'-list.");
		return ACCESS_TYPE_NONE;
	}
	int i;
	for (i = 0; i < num_hosts; i++) {
		const char *host_ip;
		char *host_key;

		host_ip = config_setting_get_string_elem(setting, i);
		host_key = strchr(host_ip, ':');
		if (host_key == NULL) {
			warnx("Invalid host:key in conf '%s'", host_ip);
			continue;
		}
		*host_key = '\0';
		host_key++;
		if (strcmp(host_ip, remoteaddr) == 0 || strcmp(host_ip, "all") == 0) {
			if (strcmp(host_key, secret_key) == 0)
				return ACCESS_TYPE_FULL;
		}
	}
	return ACCESS_TYPE_NONE;
}

void cgi_set_defaults(struct QueryDataForamt *qdata) {
#ifdef BLACK_BOKS
	qdata->anonymous = 0;
#endif
	char *user = getenv("REDIRECT_REMOTE_USER");
	if (user == NULL)
		qdata->search_user[0] = '\0';
	else
		strscpy(qdata->search_user, user, sizeof(qdata->search_user));

	qdata->subname[0] = '\0';
	qdata->orderby[0] = '\0';
	qdata->filterOn = 1;
	qdata->opensearch = 0;
	qdata->version = 2.0;
	qdata->navmenucfg[0] = '\0';
	qdata->MaxsHits = DefultMaxsHits;
	

	// legacy
	qdata->AmazonAssociateTag[0] = '\0';
	qdata->AmazonSubscriptionId[0] = '\0';
	qdata->rankUrl[0] = '\0';
}

// parameters that are handled the
// same way for both limited and full access
void cgi_fetch_common(struct QueryDataForamt *qdata, int *noDocType) {
	if (cgi_getentrystr("query") == NULL)
		die(2,"","No query provided.");
	
	strscpy(qdata->query, cgi_getentrystr("query"), sizeof(qdata->query));

        *noDocType = (cgi_getentrystr("noDoctype") == NULL) ? 1 : 0;

	int tmpint;
	qdata->start = ((tmpint = cgi_getentryint("start")) != 0) 
		? tmpint
		: 1;


	if ((tmpint = cgi_getentryint("opensearch")) != 0)
		qdata->opensearch = cgi_getentryint("opensearch");

}



void cgi_fetch_limited(struct QueryDataForamt *qdata, char *remoteaddr) {
	
	// Assuming remote address is from logged in user (API access)
	strscpy(qdata->userip, remoteaddr, sizeof qdata->userip);

	char *tmpstr;
	if ((tmpstr = getenv("HTTP_ACCEPT_LANGUAGE")) != NULL)
		strscpy(qdata->HTTP_ACCEPT_LANGUAGE, tmpstr, sizeof(qdata->HTTP_ACCEPT_LANGUAGE));
	else {
		qdata->HTTP_ACCEPT_LANGUAGE[0] = '\0';
		warnx("HTTP_ACCEPT_LANGUAGE not available");
	}
	if ((tmpstr = getenv("HTTP_USER_AGENT")) != NULL)
		strscpy(qdata->HTTP_USER_AGENT, tmpstr, sizeof(qdata->HTTP_USER_AGENT));
	else {
		qdata->HTTP_USER_AGENT[0] = '\0';
		warnx("HTTP_USER_AGENT not available");
	}
	qdata->HTTP_REFERER[0] = '\0';

	

}


void _cgistr_to_str(char *dst, char *cgi_key, size_t dst_len) {
	const char *tmpstr;
	if ((tmpstr = cgi_getentrystr(cgi_key)) != NULL)
		strscpy(dst, tmpstr, dst_len);
	else {
		dst[0] = '\0';
		warnx("%s not provided", cgi_key);
	}
}

void cgi_fetch_full(struct QueryDataForamt *qdata) {
	const char *tmpstr;
#ifdef BLACK_BOKS
	if (cgi_getentryint("anonymous") != 0) {
		qdata->anonymous = 1;
		strscpy(qdata->search_user, ANONYMOUS_USER, sizeof(qdata->search_user));
	}
	if (!qdata->anonymous) {
#endif
		if ((tmpstr = cgi_getentrystr("search_bruker")) != NULL) {
			strscpy(qdata->search_user, tmpstr, sizeof(qdata->search_user));
		}
		if (qdata->search_user[0] == '\0')
			errx(1, "search_bruker missing");
#ifdef BLACK_BOKS
	}
#endif
	if ((tmpstr = cgi_getentrystr("subname")) != NULL) {
		/* 25.05.09: eirik
		 * Subname has changed it's meaning.
		 * If subname is non-empty we only want these collection.
		 * Empty(strlen(str) == 0) is everyone
		 */
		strscpy(qdata->subname, tmpstr, sizeof(qdata->subname));
	}

	// Assuming remote address is *not* from remote user,
	// but from our client (webclient) or similar.
	if ((tmpstr = cgi_getentrystr("userip")) != NULL) {
		strscpy(qdata->userip, tmpstr, sizeof(qdata->userip));
	}
	else {
		warnx("User IP not provided.");
		qdata->userip[0] = '\0';
	}

	if ((tmpstr = cgi_getentrystr("orderby")) != NULL)
		strscpy(qdata->orderby, tmpstr, sizeof(qdata->orderby));

	int tmpint;
	if (tmpint = cgi_getentryint("filter")) 
		qdata->filterOn = tmpint;

	_cgistr_to_str(qdata->HTTP_ACCEPT_LANGUAGE, "HTTP_ACCEPT_LANGUAGE", sizeof qdata->HTTP_ACCEPT_LANGUAGE);
	_cgistr_to_str(qdata->HTTP_USER_AGENT, "HTTP_USER_AGENT", sizeof qdata->HTTP_USER_AGENT);
	_cgistr_to_str(qdata->HTTP_REFERER, "HTTP_REFERER", sizeof qdata->HTTP_REFERER);


	if (cgi_getentrydouble("version") != 0) 
		qdata->version = cgi_getentrydouble("version");

	if ((tmpstr = cgi_getentrystr("navmenucfg")) != NULL) {
		base64_decode(qdata->navmenucfg, tmpstr, sizeof qdata->navmenucfg);
	}
	else {
		warnx("navmenucfg not provided");
		qdata->navmenucfg[0] = '\0';
	}

	if ((tmpint = cgi_getentryint("maxhits")) != 0) 
		qdata->MaxsHits = tmpint;
}
