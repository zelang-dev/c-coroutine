#include "../include/coroutine.h"

int co_main(int argc, char *argv[]) {
    http_t *parser = http_for(HTTP_REQUEST, NULL, 1.1);
    string request = co_concat_by(13, "GET /index.html HTTP/1.1", CRLF,
                                  "Host: url.com", CRLF,
                                  "Accept: */*", CRLF,
                                  "User-Agent: uv_client", CRLF,
                                  "X-Powered-By: ZeLang", CRLF,
                                  "Connection: close", CRLF, CRLF);

    if (is_str_eq(request, http_request(parser, HTTP_GET, "http://url.com/index.html", NULL, NULL, NULL, "x-powered-by=ZeLang;")))
        printf("\nThey match!\n\n%sThe uri is: %s\n", request, parser->uri);

    char raw[] = "POST /path?free=one&open=two HTTP/1.1\n\
 User-Agent: PHP-SOAP/BeSimpleSoapClient\n\
 Host: url.com:80\n\
 Accept: */*\n\
 Accept-Encoding: deflate, gzip\n\
 Content-Type:text/xml; charset=utf-8\n\
 Content-Length: 1108\n\
 Expect: 100-continue\n\
 \n\n\
 <b>hello world</b>";

    parse_http(parser, raw);

    CO_ASSERT(is_str_eq("PHP-SOAP/BeSimpleSoapClient", get_header(parser, "User-Agent")));
    CO_ASSERT(is_str_eq("url.com:80", get_header(parser, "Host")));
    CO_ASSERT(is_str_eq("*/*", get_header(parser, "Accept")));
    CO_ASSERT(is_str_eq("deflate, gzip", get_header(parser, "Accept-Encoding")));
    CO_ASSERT(is_str_eq("text/xml; charset=utf-8", get_header(parser, "Content-Type")));
    CO_ASSERT(is_str_eq("1108", get_header(parser, "Content-Length")));
    CO_ASSERT(is_str_eq("100-continue", get_header(parser, "Expect")));
    CO_ASSERT(is_str_eq("utf-8", get_variable(parser, "Content-Type", "charset")));
    CO_ASSERT(is_str_eq("", get_variable(parser, "Expect", "charset")));

    CO_ASSERT(has_header(parser, "Pragma") == false);

    CO_ASSERT(is_str_eq("<b>hello world</b>", parser->body));
    CO_ASSERT(is_str_eq("one", get_parameter(parser, "free")));
    CO_ASSERT(is_str_eq("POST", parser->method));
    CO_ASSERT(is_str_eq("/path", parser->path));
    CO_ASSERT(is_str_eq("HTTP/1.1", parser->protocol));

    return 0;
}
