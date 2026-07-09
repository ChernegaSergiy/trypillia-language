<script lang="ts">
	let { code = '' }: { code?: string } = $props();

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
</script>

<span class="inline-code">
	<code>{code}</code>
	<button class="copy-btn" onclick={copyCode} aria-label="Копіювати код">
		{copied ? '✓' : '⧉'}
	</button>
</span>
