<script lang="ts">
	import { base } from '$app/paths';
	import { modules } from '$lib/stdlib';
</script>

<svelte:head>
	<title>Стандартна бібліотека — Trypillia</title>
</svelte:head>

<section>
	<h2>Стандартна бібліотека</h2>
	<p>Модулі та їхні функції. Оберіть модуль, щоб переглянути всі його функції.</p>
</section>

<div class="pkg-list">
	{#each modules as m (m.name)}
		<div class="pkg-card">
			<div class="pkg-header">
				<a href={`${base}/docs/stdlib/${m.name}`} class="pkg-title">{m.name}</a>
				<code>{m.functions.length}</code>
			</div>
			<p class="pkg-desc">{m.description}</p>
		</div>
	{/each}
</div>
