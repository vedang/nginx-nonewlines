/*
 * This NginX module strips the HTML being served by NginX of all newline
 * ('\n', '\r') and whitespace ('\t' and extra spaces) characters,
 * thereby saving on the bandwidth required to serve the page
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

#define SC_OFF  "<!--SC_OFF-->"
#define SC_ON   "<!--SC_ON-->"
#define SC_OFF_LEN  (sizeof(SC_OFF)-1)
#define SC_ON_LEN   (sizeof(SC_ON)-1)

/* Declarations */

typedef struct {
        unsigned char state;
} ngx_http_no_newlines_ctx_t;

typedef struct {
        ngx_flag_t enable; /* A flag to enable or disable module functionality. */
} ngx_http_no_newlines_conf_t;

typedef enum {
        state_text_compress = 0,
        state_text_no_compress
} ngx_http_no_newlines_state_e;


static void *ngx_http_no_newlines_create_conf (ngx_conf_t *cf);
static char *ngx_http_no_newlines_merge_conf (ngx_conf_t *cf,
                                              void *parent,
                                              void *child);
static ngx_int_t ngx_http_no_newlines_header_filter (ngx_http_request_t *r);
static ngx_int_t ngx_http_no_newlines_body_filter (ngx_http_request_t *r,
                                                   ngx_chain_t *in);
static ngx_int_t ngx_http_no_newlines_filter_init (ngx_conf_t *cf);
static void ngx_http_no_newlines_strip_buffer (ngx_buf_t *buffer,
                                               ngx_http_no_newlines_ctx_t *ctx);
static ngx_int_t ngx_is_space (u_char* c);


/* Module directives */
static ngx_command_t  ngx_http_no_newlines_commands[] = {
        { ngx_string ("no_newlines"),
          NGX_HTTP_MAIN_CONF | NGX_HTTP_SRV_CONF | NGX_HTTP_LOC_CONF | NGX_CONF_FLAG,
          ngx_conf_set_flag_slot,
          NGX_HTTP_LOC_CONF_OFFSET,
          offsetof(ngx_http_no_newlines_conf_t, enable),
          NULL },

        ngx_null_command
};


/* The Module Context - for managing the configurations */
static ngx_http_module_t  ngx_http_no_newlines_module_ctx = {
        NULL,                             /* pre-configuration */
        ngx_http_no_newlines_filter_init, /* post-configuration */

        NULL,                             /* create main configuration */
        NULL,                             /* init main configuration */

        NULL,                             /* create server configuration */
        NULL,                             /* merge server configuration */

        ngx_http_no_newlines_create_conf, /* create location configuration */
        ngx_http_no_newlines_merge_conf   /* merge location configuration */
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


static ngx_http_output_header_filter_pt  ngx_http_next_header_filter;
static ngx_http_output_body_filter_pt    ngx_http_next_body_filter;


/* Function definitions start here */


static void *ngx_http_no_newlines_create_conf (ngx_conf_t *cf)
{
        ngx_http_no_newlines_conf_t *conf;

        conf = ngx_pcalloc (cf->pool, sizeof(ngx_http_no_newlines_conf_t));
        if (conf == NULL) {
                return NGX_CONF_ERROR;
        }

        conf->enable = NGX_CONF_UNSET;

        return conf;
}


static char *ngx_http_no_newlines_merge_conf (ngx_conf_t *cf,
                                              void *parent,
                                              void *child)
{
        ngx_http_no_newlines_conf_t *prev = parent;
        ngx_http_no_newlines_conf_t *conf = child;

        ngx_conf_merge_value(conf->enable, prev->enable, 0);

        return NGX_CONF_OK;
}


static ngx_int_t ngx_http_no_newlines_filter_init (ngx_conf_t *cf)
{
        ngx_http_next_header_filter = ngx_http_top_header_filter;
        ngx_http_top_header_filter = ngx_http_no_newlines_header_filter;

        ngx_http_next_body_filter = ngx_http_top_body_filter;
        ngx_http_top_body_filter = ngx_http_no_newlines_body_filter;

        return NGX_OK;
}


static ngx_int_t ngx_http_no_newlines_header_filter (ngx_http_request_t *r)
{
        ngx_http_no_newlines_ctx_t   *ctx;  /* to maintain state */
        ngx_http_no_newlines_conf_t  *conf; /* to check whether module is enabled or not */

        conf = ngx_http_get_module_loc_conf (r, ngx_http_no_newlines_module);

        /* step 1: decide whether to operate */
        if ((r->headers_out.status != NGX_HTTP_OK &&
             r->headers_out.status != NGX_HTTP_FORBIDDEN &&
             r->headers_out.status != NGX_HTTP_NOT_FOUND) ||
            r->header_only ||
            r->headers_out.content_type.len == 0 ||
            (r->headers_out.content_encoding &&
             r->headers_out.content_encoding->value.len) ||
            conf->enable == 0) {
                return ngx_http_next_header_filter(r);
        }

        if (ngx_strncasecmp(r->headers_out.content_type.data, (u_char *)"text/html", sizeof("text/html") - 1) != 0) {
                return ngx_http_next_header_filter(r);
        }

        /* step 2: operate on the header */
        ctx = ngx_pcalloc(r->pool, sizeof(ngx_http_no_newlines_ctx_t));
        if (ctx == NULL) {
                return NGX_ERROR;
        }

        ngx_http_set_ctx(r, ctx, ngx_http_no_newlines_module);

        ngx_http_clear_content_length(r);
        ngx_http_clear_accept_ranges(r);
        r->main_filter_need_in_memory = 1;

        /* step 3: call the next filter */
        return ngx_http_next_header_filter(r);
}


static ngx_int_t ngx_http_no_newlines_body_filter (ngx_http_request_t *r,
                                                   ngx_chain_t *in)
{
        ngx_http_no_newlines_ctx_t *ctx;
        ngx_chain_t *chain_link;

        /* Get the current context */
        ctx = ngx_http_get_module_ctx (r, ngx_http_no_newlines_module);

        if (ctx == NULL) {
                return ngx_http_next_body_filter(r, in);
        }

        /* For each buffer in the chain link, remove all the newlines */
        for (chain_link = in; chain_link; chain_link = chain_link->next) {
                ngx_http_no_newlines_strip_buffer (chain_link->buf, ctx);
        }

        /* Pass the chain to the next output filter */
        return ngx_http_next_body_filter(r, in);
}


static void ngx_http_no_newlines_strip_buffer (ngx_buf_t *buffer,
                                               ngx_http_no_newlines_ctx_t *ctx)
{
        u_char *reader = NULL;
        u_char *writer = NULL;
        u_char *t = NULL;
        ngx_int_t space_eaten = 0;

        for (writer = buffer->pos, reader = buffer->pos; reader < buffer->last; reader++) {
                switch(ctx->state) {
                case state_text_compress:

                        if(ngx_is_space(reader)) {
                            space_eaten = 1;
                            reader++;
                            while (reader < buffer->last && ngx_is_space (reader)) reader++;
                             if(reader >= buffer->last) /* FIXME: Does it make sense to strip if buffer isn't sanitized? */
                                goto out;
                        }

                        /* unless next char is '<', add one space for all eaten */
                        if (space_eaten && *reader != '<') {
                                *writer++ = ' ';
                        }
                        space_eaten = 0;

                        if(*reader == '>') {
                                *writer++ = *reader++;
                                while (reader < buffer->last && ngx_is_space (reader)) {
                                        reader++;
                                }
                                if(reader >= buffer->last)
                                    goto out;
                        }
                        
                        /* does the next part of the string match the SC_OFF label? */
                        t = reader;
                        if ( (buffer->last - reader) >= SC_OFF_LEN
                            &&
                             ngx_strncasecmp (t, SC_OFF, SC_OFF_LEN) == 0) {
                                ctx->state = state_text_no_compress;
                                reader += SC_OFF_LEN;
                        }
                        break;

                case state_text_no_compress:
                        /* ignore newlines and whitespace while we are */
                        /* displaying pre-formatted text */
                        /* look for SC_ON tag */
                        t = reader;
                        if ( (buffer->last - reader) >= SC_ON_LEN
                             &&
                             ngx_strncasecmp (t, SC_ON, SC_ON_LEN) == 0) {
                                ctx->state = state_text_compress;
                                reader += SC_ON_LEN;
                        }
                        break;

                default:
                        break;
                }

                if (reader < buffer->last) {
                        *writer++ = *reader;
                }
        }
    out:
        buffer->last = writer;
}


static ngx_int_t ngx_is_space (u_char* c)
{
        if (*c == '\n' ||
            *c == '\r' ||
            *c == '\t' ||
            (*c == ' ' && *(c + 1) == ' ')) {
                /* Leave the last space so that links on the page don't */
                /* get messed up */
                return 1;
        }
        return 0;
}
