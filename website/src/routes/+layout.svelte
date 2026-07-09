<script lang="ts">
	import Header from '$lib/components/Header.svelte';
	import Footer from '$lib/components/Footer.svelte';
	import '$lib/styles/global.css';

	let { children } = $props();
</script>

<Header />
{@render children()}
<Footer />
