#!/bin/sh
set -eu

binary=${1:-bin/hhy-html}
output=$(
    printf '%s\n' \
      '{"type":"handshake","request_id":"handshake","extension_id":"html","protocol_version":"1.0","runtime_version":"1.1.1"}' \
      '{"type":"call","request_id":"1","extension_id":"html","protocol_version":"1.0","callable":"html.text","arguments":["<p> HHY &amp; Flow </p>","p"]}' \
      '{"type":"call","request_id":"2","extension_id":"html","protocol_version":"1.0","callable":"html.attr_all","arguments":["<a href=\"/a\">A</a><a>B</a><a href=\"/c\">C</a>","a","href"]}' \
      '{"type":"call","request_id":"3","extension_id":"html","protocol_version":"1.0","callable":"html.extract","arguments":["<article><h2>One</h2><a href=\"/1\">Go</a></article><article><h2>Two</h2></article>","article",{"title":{"selector":"h2","value":"text"},"url":{"selector":"a","value":"attr","name":"href"}}]}' \
      '{"type":"call","request_id":"4","extension_id":"html","protocol_version":"1.0","callable":"html.exists","arguments":["<div class=\"ready\"></div>","div.ready"]}' \
      '{"type":"call","request_id":"5","extension_id":"html","protocol_version":"1.0","callable":"html.text","arguments":["<p>x</p>","["]}' \
      '{"type":"shutdown","request_id":"shutdown","extension_id":"html","protocol_version":"1.0"}' |
    "$binary" --protocol 1
)

printf '%s\n' "$output" | grep -F '"extension_version":"0.1.0"' >/dev/null
printf '%s\n' "$output" | grep -F '"value":"HHY & Flow"' >/dev/null
printf '%s\n' "$output" | grep -F '"value":["/a","/c"]' >/dev/null
printf '%s\n' "$output" | grep -F '"value":[{"title":"One","url":"/1"},{"title":"Two","url":null}]' >/dev/null
printf '%s\n' "$output" | grep -F '"value":true' >/dev/null
printf '%s\n' "$output" | grep -F '"code":"HTML_INVALID_SELECTOR"' >/dev/null
