<script lang="ts">
	let {
		code = '',
		lang = 'trypillia',
		class: className = ''
	}: { code?: string; lang?: string; class?: string } = $props();

	let copied = $state(false);

	async function copyCode() {
		try {
			await navigator.clipboard.writeText(code);
			copied = true;
			setTimeout(() => (copied = false), 1500);
		} catch {
			// ignore
		}
	}

	const keywords = new Set([
		'fn', 'let', 'const', 'if', 'else', 'elif', 'return', 'while', 'for', 'loop',
		'match', 'in', 'as', 'struct', 'enum', 'pub', 'import', 'from', 'true',
		'false', 'null', 'void', 'type', 'trait', 'impl', 'new', 'await', 'async',
		'yield', 'break', 'continue', 'mut'
	]);

	function escape_html(s: string): string {
		return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
	}

	function highlight(src: string): string {
		let out = '';
		let last = 0;
		const re =
			/(\/\/[^\n]*)|("(?:[^"\\]|\\.)*")|(\b\d+(?:\.\d+)?\b)|([A-Za-z_]\w*)(?=\s*\()|(\b(?:fn|let|const|if|else|elif|return|while|for|loop|match|in|as|struct|enum|pub|import|from|true|false|null|void|type|trait|impl|new|await|async|yield|break|continue|mut)\b)/g;

		let m: RegExpExecArray | null;
		while ((m = re.exec(src)) !== null) {
			out += escape_html(src.slice(last, m.index));
			const [full, comment, str, num, fncall, kw] = m;
			if (comment) out += `<span class="comment">${escape_html(comment)}</span>`;
			else if (str) out += `<span class="str">${escape_html(str)}</span>`;
			else if (num) out += `<span class="num">${escape_html(num)}</span>`;
			else if (fncall) out += `<span class="fn">${escape_html(fncall)}</span>`;
			else if (kw) out += `<span class="kw">${escape_html(kw)}</span>`;
			last = m.index + full.length;
		}
		out += escape_html(src.slice(last));
		return out;
	}
</script>

<div class="code-block {className}">
	<button class="copy-btn" onclick={copyCode} aria-label="Копіювати код">
		{copied ? 'Скопійовано' : 'Копіювати'}
	</button>
	<pre><code>{@html lang === 'trypillia' ? highlight(code) : escape_html(code)}</code></pre>
</div>
