<script lang="ts">
	import { base } from '$app/paths';

	let {
		items
	}: {
		items: { href?: string; label: string }[]
	} = $props();
</script>

<nav class="breadcrumb">
	{#each items as item, i (i)}
		{#if item.href}
			<a href={`${base}${item.href}`}>{item.label}</a>
		{:else}
			<span>{item.label}</span>
		{/if}
		{#if i < items.length - 1}
			{' / '}
		{/if}
	{/each}
</nav>
