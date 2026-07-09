<script lang="ts">
	import { base } from '$app/paths';
	import { getModule } from '$lib/docs-nav';

	let { slug }: { slug: string } = $props();

	const module = $derived(getModule(slug));
</script>

<svelte:head>
	<title>{module ? module.title : slug} — Стандартна бібліотека Trypillia</title>
</svelte:head>

{#if module}
	<nav class="breadcrumb">
		<a href={`${base}/docs/stdlib`}>Стандартна бібліотека</a> / <span>{module.title}</span>
	</nav>

	<section>
		<h2>{module.title}</h2>
		<p>{module.description}</p>
	</section>

	<section>
		<h3>Методи ({module.methods.length})</h3>
		<div class="pkg-list">
			{#each module.methods as fn (fn.name)}
				<div class="pkg-card">
					<div class="pkg-header">
						<a href={`${base}/docs/stdlib/${module.slug}/${fn.name}`} class="pkg-title">
							{fn.label}
						</a>
					</div>
					<p class="pkg-desc">{fn.summary}</p>
				</div>
			{/each}
		</div>
	</section>
{/if}
