<script lang="ts">
	import { base } from '$app/paths';
	import CodeBlock from '$lib/components/CodeBlock.svelte';
	import type { PageData } from './$types';

	let { data }: { data: PageData } = $props();
	const module = $derived(data.module);
</script>

<svelte:head>
	<title>{module.name} — Стандартна бібліотека Trypillia</title>
</svelte:head>

<nav class="breadcrumb">
	<a href={`${base}/docs/stdlib`}>Стандартна бібліотека</a> / <span>{module.name}</span>
</nav>

<section>
	<h2>{module.name}</h2>
	<p>{module.description}</p>
</section>

<section>
	<h3>Функції ({module.functions.length})</h3>
	<div class="pkg-list">
		{#each module.functions as fn (fn.key)}
			<div class="pkg-card">
				<div class="pkg-header">
					<a href={`${base}/docs/stdlib/${module.name}/${fn.name}`} class="pkg-title">{fn.name}</a>
				</div>
				<CodeBlock code={fn.signature} />
				<p class="pkg-desc">{fn.doc}</p>
			</div>
		{/each}
	</div>
</section>
