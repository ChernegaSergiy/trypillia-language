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

  const serverOptions: ServerOptions = {
    run: { command: serverCommand },
    debug: { command: serverCommand }
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
