import type { ReactNode } from "react";

const keywords = new Set([
  "let", "mut", "fn", "import", "export", "from", "as",
  "if", "else", "for", "in", "while", "return", "break", "continue",
  "try", "catch", "throw", "attempt", "and", "or", "not"
]);

const constants = new Set(["true", "false", "null"]);
const builtins = new Set([
  "append", "collect", "contains", "count", "distinct", "encode_json", "exit", "first",
  "flat_map", "get", "group_by", "inspect", "is_type", "length", "map", "parse_json",
  "pick", "print", "print_error", "put", "read_text", "reduce", "regex_captures",
  "regex_match", "require", "run", "save_text", "send", "send_to", "sort_by", "stream",
  "take", "timeout", "type", "where"
]);
const operators = ["|>", "??", "->", "..", "==", "!=", "<=", ">=", "+", "-", "*", "/", "%", "=", "<", ">", "."];
const punctuation = new Set(["(", ")", "[", "]", "{", "}", ",", ":", ";"]);

type TokenKind = "keyword" | "function" | "number" | "boolean" | "comment" | "operator" | "punctuation" | "string" | "regex";
type Token = { text: string; kind?: TokenKind };

function tokenizeHhy(code: string): Token[] {
  const tokens: Token[] = [];
  let index = 0;

  const push = (text: string, kind?: TokenKind) => tokens.push({ text, kind });

  while (index < code.length) {
    const rest = code.slice(index);
    const char = code[index];

    if (/\s/.test(char)) {
      const match = rest.match(/^\s+/)![0];
      push(match);
      index += match.length;
      continue;
    }

    if (char === "#") {
      const end = code.indexOf("\n", index);
      const text = code.slice(index, end === -1 ? code.length : end);
      push(text, "comment");
      index += text.length;
      continue;
    }

    if (char === '"') {
      let end = index + 1;
      while (end < code.length) {
        if (code[end] === "\\") end += 2;
        else if (code[end++] === '"') break;
        else end += 0;
      }
      push(code.slice(index, end), "string");
      index = end;
      continue;
    }

    if (char === "/" && !/\s/.test(code[index + 1] ?? "")) {
      let end = index + 1;
      let closed = false;
      while (end < code.length && code[end] !== "\n") {
        if (code[end] === "\\") end += 2;
        else if (code[end++] === "/") { closed = true; break; }
        else end += 0;
      }
      if (closed) {
        while (/[imsu]/.test(code[end] ?? "")) end++;
        push(code.slice(index, end), "regex");
        index = end;
        continue;
      }
    }

    const number = rest.match(/^(?:0x[\da-fA-F]+|0b[01]+|\d+(?:\.\d+)?(?:e[+-]?\d+)?)(?:kib|mib|gib|tib|kb|mb|gb|tb|min|ns|us|ms|s|h|d|b|%)?/i)?.[0];
    if (number) {
      push(number, "number");
      index += number.length;
      continue;
    }

    const identifier = rest.match(/^[A-Za-z_][A-Za-z0-9_]*/)?.[0];
    if (identifier) {
      const after = code.slice(index + identifier.length).match(/^\s*/)?.[0].length ?? 0;
      const next = code[index + identifier.length + after];
      const kind = keywords.has(identifier)
        ? "keyword"
        : constants.has(identifier)
          ? "boolean"
          : builtins.has(identifier) || next === "("
            ? "function"
            : undefined;
      push(identifier, kind);
      index += identifier.length;
      continue;
    }

    const operator = operators.find((candidate) => rest.startsWith(candidate));
    if (operator) {
      push(operator, "operator");
      index += operator.length;
      continue;
    }

    push(char, punctuation.has(char) ? "punctuation" : undefined);
    index++;
  }

  return tokens;
}

export function HhyCode({ code, lineNumbers = false }: { code: string; lineNumbers?: boolean }) {
  const renderTokens = (source: string, keyPrefix: string): ReactNode => tokenizeHhy(source).map((token, index) => token.kind
    ? <span className={`hhy-${token.kind}`} key={`${keyPrefix}-${index}`}>{token.text}</span>
    : token.text);

  if (!lineNumbers) return <>{renderTokens(code, "token")}</>;

  return <>{code.split("\n").map((line, index) => <span className="hhy-line" key={index}>
    <span className="hhy-line-number" aria-hidden>{index + 1}</span>
    <span className="hhy-line-code">{renderTokens(line, `line-${index}`)}</span>
  </span>)}</>;
}
