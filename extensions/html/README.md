# HTML process extension

The official `html` extension parses untrusted HTML with Lexbor and evaluates
CSS selectors without performing network or filesystem effects.

## Build and install

```sh
# macOS
brew install jansson lexbor

make -C extensions/html
./build/hhy install ./extensions/html
```

Ubuntu and Debian builds require the Jansson and Lexbor development packages.
The extension links to `libjansson` and `liblexbor`.

## API

```text
html.text(String html, String selector, Map?) -> String | Null
html.text_all(String html, String selector, Map?) -> List<String>
html.attr(String html, String selector, String name, Map?) -> String | Null
html.attr_all(String html, String selector, String name, Map?) -> List<String>
html.exists(String html, String selector) -> Bool
html.extract(String html, String selector, Map schema, Map?) -> List<Map>
```

`text`, `text_all`, `attr`, and `attr_all` accept `{ trim: Bool }`. Collection
operations also accept `{ max_results: Int }`; the default is 1000 and the hard
limit is 10000. Input HTML is limited to 768 KiB so responses remain below the
1 MiB Process Extension Protocol message limit.

`extract` parses the HTML document once, selects repeated root elements, and
projects each root into a Map. A field selector is evaluated relative to its
root. Use an empty selector to read the root element itself.

```hhy
import html

let source = """
<section class="products">
  <article class="product"><h2> HHY One </h2><a href="/one">Open</a></article>
  <article class="product"><h2>HHY Two</h2><a href="/two">Open</a></article>
</section>
"""

html.extract(source, "article.product", {
    title: { selector: "h2", value: "text" },
    url: { selector: "a", value: "attr", name: "href" }
}) |> print
```

The extension deliberately does not fetch URLs. HHY Core remains responsible
for HTTP, TLS, timeout, retry, cancellation, redaction, and dry-run behavior.
