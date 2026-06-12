import * as path from 'path';
import { workspace, ExtensionContext } from 'vscode';

import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions
} from 'vscode-languageclient/node';

let client: LanguageClient;

export function activate(context: ExtensionContext) {
  // Path to the trypillia-lsp executable
  const serverCommand = path.join(context.extensionPath, '..', 'build', 'trypillia-lsp');
  const docsPath = path.join(context.extensionPath, '..', 'src', 'lsp', 'native_docs.json');

  const serverOptions: ServerOptions = {
    run: { command: serverCommand, args: ["--docs", docsPath] },
    debug: { command: serverCommand, args: ["--docs", docsPath] }
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: 'file', language: 'trypillia' }],
    synchronize: {
      fileEvents: workspace.createFileSystemWatcher('**/.try')
    }
  };

  client = new LanguageClient(
    'trypilliaLanguageServer',
    'Trypillia Language Server',
    serverOptions,
    clientOptions
  );

  client.start();
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) {
    return undefined;
  }
  return client.stop();
}
