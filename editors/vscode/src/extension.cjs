const vscode = require("vscode");
const { LanguageClient } = require("vscode-languageclient/node");

let client;

function activate(context) {
  const configuration = vscode.workspace.getConfiguration("hhy");
  const serverModule = context.asAbsolutePath("lsp/server.mjs");
  const serverOptions = {
    command: process.execPath,
    args: [serverModule],
    options: {
      env: {
        ...process.env,
        ELECTRON_RUN_AS_NODE: "1",
        HHY_BIN: configuration.get("executablePath", "hhy"),
      },
    },
  };
  client = new LanguageClient(
    "hhyLanguageServer",
    "HHY Language Server",
    serverOptions,
    { documentSelector: [{ scheme: "file", language: "hhy" }] },
  );
  context.subscriptions.push(client);
  client.start();
}

async function deactivate() {
  if (client) await client.stop();
}

module.exports = { activate, deactivate };
