<script lang="ts">
	import DocsBreadcrumb from '$lib/components/DocsBreadcrumb.svelte';

	let {
		module,
		title,
		name
	}: { module: string; title: string; name: string } = $props();
</script>

<DocsBreadcrumb
	items={[
		{ href: '/docs', label: 'Документація' },
		{ href: '/docs/stdlib', label: 'Стандартна бібліотека' },
		{ href: `/docs/stdlib/${module}`, label: title },
		{ label: name }
	]}
/>
