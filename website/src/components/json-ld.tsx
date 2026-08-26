import { Fragment } from "react";

type JsonLdProps = {
  data: Record<string, unknown> | Array<Record<string, unknown>>;
};

export function JsonLd({ data }: JsonLdProps) {
  const entries = Array.isArray(data) ? data : [data];

  return entries.map((entry, index) => (
    <Fragment key={index}>
      <script
        type="application/ld+json"
        dangerouslySetInnerHTML={{ __html: JSON.stringify(entry).replace(/</g, "\\u003c") }}
      />
    </Fragment>
  ));
}
