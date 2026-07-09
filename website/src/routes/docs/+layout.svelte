<script lang="ts">
	import { page } from '$app/stores';
	import { base } from '$app/paths';
	import { modules } from '$lib/stdlib';

	let { children } = $props();

	const path = $derived($page.url.pathname);
</script>

<div class="container docs-layout">
	<aside class="docs-sidebar">
		<nav>
			<p class="docs-nav-title">Документація</p>
			<a href={`${base}/docs/language`} class:active={path === '/docs/language'}>Мова Trypillia</a>

			<p class="docs-nav-title">Стандартна бібліотека</p>
			<ul class="docs-module-list">
				{#each modules as m (m.name)}
					<li>
						<a
							href={`${base}/docs/stdlib/${m.name}`}
							class:active={path.startsWith(`/docs/stdlib/${m.name}`)}
						>
							{m.name}
							<span class="docs-module-count">{m.functions.length}</span>
						</a>
					</li>
				{/each}
			</ul>
		</nav>
	</aside>

	<main class="docs-main">
		{@render children()}
	</main>
</div>
