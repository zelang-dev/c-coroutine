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
        printf("\nThey match!\n\n%s\n The uri is: %s\n", request, parser->uri);

    char raw[] = "POST /path?free=one&open=two HTTP/1.1\n\
User-Agent: PHP-SOAP/BeSimpleSoapClient\n\
Host: url.com:80\n\
Accept: */*\n\
Accept-Encoding: deflate, gzip\n\
Content-Type:text/xml; charset=utf-8\n\
Content-Length: 1108\n\
Expect: 100-continue\n\
\n\
<b>hello world</b>";
}
/*

$parser->parse($raw);
$this->assertEquals("PHP-SOAP/\BeSimple\SoapClient", $parser->getHeader("User-Agent"));
$this->assertEquals("url.com:80", $parser->getHeader("Host"));
$this->assertEquals("*//*", $parser->getHeader("Accept"));
$this->assertEquals("deflate, gzip", $parser->getHeader("Accept-Encoding"));
$this->assertEquals("text/xml; charset=utf-8", $parser->getHeader("Content-Type"));
$this->assertEquals("1108", $parser->getHeader("Content-Length"));
$this->assertEquals("100-continue", $parser->getHeader("Expect"));

$this->assertEquals("utf-8", $parser->getVariable("Content-Type", "charset"));
$this->assertEquals("", $parser->getVariable("Expect", "charset"));
$this->assertFalse($parser->hasHeader("Pragma"));
$this->assertEquals("<b>hello world</b>", $parser->getBody());

$default = $parser->getHeader("n/a");
$this->assertEquals("", $default);

$this->assertCount(2, $parser->getParameter());
$this->assertEquals("one", $parser->getParameter("free"));
$this->assertEquals("POST", $parser->getMethod());
$this->assertEquals("/path", $parser->getPath());
$this->assertEquals("HTTP/1.1", $parser->getProtocol());
*/
