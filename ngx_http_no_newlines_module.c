/*
 * This NginX module strips the HTML being served by NginX of all newline
 * ('\n', '\r') characters, thereby saving on the bandwidth
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* Module directives */
static ngx_command_t  ngx_http_no_newlines_commands[] = {
    { ngx_string ("no_newlines"),
      NGX_HTTP_LOC_CONF | NGX_CONF_NOARGS,
      ngx_http_no_newlines,
      0,
      0,
      NULL },

    ngx_null_command
};

/* The Module Context - for managing the configurations */
static ngx_http_module_t  ngx_http_no_newlines_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    NULL,                          /* create location configuration */
    NULL                           /* merge location configuration */
};

