<script lang="ts">
	import { base } from '$app/paths';
	import { modules } from '$lib/docs-nav';
</script>

<svelte:head>
	<title>Стандартна бібліотека — Trypillia</title>
</svelte:head>

<section>
	<h2>Стандартна бібліотека</h2>
	<p>Модулі та їхні методи. Оберіть модуль, щоб переглянути всі його методи.</p>
</section>

<div class="pkg-list">
	{#each modules as m (m.slug)}
		<div class="pkg-card">
			<div class="pkg-header">
				<a href={`${base}/docs/stdlib/${m.slug}`} class="pkg-title">{m.title}</a>
				<code>{m.methods.length}</code>
			</div>
			<p class="pkg-desc">{m.description}</p>
		</div>
	{/each}
</div>
