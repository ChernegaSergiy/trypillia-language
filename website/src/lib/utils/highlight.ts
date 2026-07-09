import type { HighlighterCore } from '@shikijs/types';
import type { LanguageRegistration } from '@shikijs/types';
import type { IRawGrammar } from '@shikijs/vscode-textmate';

let highlighterPromise: Promise<HighlighterCore> | null = null;

function escapeHtml(s: string): string {
	return s
		.replace(/&/g, '&amp;')
		.replace(/</g, '&lt;')
		.replace(/>/g, '&gt;');
}

async function getHighlighter(): Promise<HighlighterCore> {
	if (!highlighterPromise) {
		const { createHighlighter } = await import('shiki');
		const res = await fetch('/trypillia.tmLanguage.json');
		const grammar = (await res.json()) as IRawGrammar;

		const h = await createHighlighter({
			themes: ['github-dark'],
			langs: []
		});

		await h.loadLanguage({
			...grammar,
			name: 'trypillia'
		} satisfies LanguageRegistration);

		highlighterPromise = Promise.resolve(h);
	}
	return highlighterPromise;
}

export async function highlightTrypillia(code: string): Promise<string> {
	try {
		const h = await getHighlighter();
		const html = h.codeToHtml(code, {
			lang: 'trypillia',
			theme: 'github-dark'
		});

		const match = html.match(/<code[^>]*>([\s\S]*?)<\/code>/);
		if (match) {
			return match[1].trim();
		}
		return escapeHtml(code);
	} catch {
		return escapeHtml(code);
	}
}
