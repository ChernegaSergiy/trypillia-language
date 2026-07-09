<script lang="ts">
	import { highlightTrypillia } from '$lib/utils/highlight';

	let {
		code = '',
		lang = 'trypillia',
		class: className = '',
		dense = false
	}: { code?: string; lang?: string; class?: string; dense?: boolean } = $props();

	let copied = $state(false);
	let highlighted = $state('');

	async function copyCode() {
		try {
			await navigator.clipboard.writeText(code);
			copied = true;
			setTimeout(() => (copied = false), 1500);
		} catch {
			// ignore
		}
	}

	function escapeHtml(s: string): string {
		return s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
	}

	$effect(() => {
		if (lang === 'trypillia') {
			highlightTrypillia(code).then((v) => (highlighted = v));
		} else {
			highlighted = escapeHtml(code);
		}
	});
</script>

<div class="code-block {className}" class:dense>
	<button class="copy-btn" onclick={copyCode} aria-label="Копіювати код">
		{copied ? 'Скопійовано' : 'Копіювати'}
	</button>
	{#if dense}
		<code>{@html highlighted}</code>
	{:else}
		<pre><code>{@html highlighted}</code></pre>
	{/if}
</div>
