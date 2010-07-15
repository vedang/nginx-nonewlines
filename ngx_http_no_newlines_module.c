/*
 * This NginX module strips the HTML being served by NginX of all newline
 * ('\n', '\r') characters, thereby saving on the bandwidth
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

/* Setup function for the no_newlines directive */
static char *ngx_http_no_newlines (ngx_conf_t *cf,
                                   ngx_command_t *cmd,
                                   void *conf);
/* Handler function */
static ngx_int_t ngx_http_no_newlines_handler (ngx_http_request_t *r);

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

/* The Module Definition - the master control */
ngx_module_t ngx_http_no_newlines_module = {
    NGX_MODULE_V1,
    &ngx_http_no_newlines_module_ctx, /* module context */
    ngx_http_no_newlines_commands,    /* module directives */
    NGX_HTTP_MODULE,                  /* module type */
    NULL,                             /* init master */
    NULL,                             /* init module */
    NULL,                             /* init process */
    NULL,                             /* init thread */
    NULL,                             /* exit thread */
    NULL,                             /* exit process */
    NULL,                             /* exit master */
    NGX_MODULE_V1_PADDING
};
