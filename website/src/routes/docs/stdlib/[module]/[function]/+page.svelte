<script lang="ts">
	import { base } from '$app/paths';
	import CodeBlock from '$lib/components/CodeBlock.svelte';
	import type { PageData } from './$types';

	let { data }: { data: PageData } = $props();
	const func = $derived(data.func);
</script>

<svelte:head>
	<title>{func.module}.{func.name} — Trypillia</title>
</svelte:head>

<nav class="breadcrumb">
	<a href={`${base}/docs/stdlib`}>Стандартна бібліотека</a> /
	<a href={`${base}/docs/stdlib/${func.module}`}>{func.module}</a> /
	<span>{func.name}</span>
</nav>

<section>
	<h2>{func.module}.{func.name}</h2>
	<CodeBlock code={func.signature} />
	<p class="func-doc">{func.doc}</p>
</section>

{#if func.params.length > 0}
	<section>
		<h3>Параметри</h3>
		<table class="param-table">
			<thead>
				<tr>
					<th>Параметр</th>
					<th>Опис</th>
				</tr>
			</thead>
			<tbody>
				{#each func.params as p (p.name)}
					<tr>
						<td><code>{p.name}</code></td>
						<td>{p.doc}</td>
					</tr>
				{/each}
			</tbody>
		</table>
	</section>
{/if}

{#if func.returns}
	<section>
		<h3>Повертає</h3>
		<p><code>{func.returns}</code></p>
	</section>
{/if}
