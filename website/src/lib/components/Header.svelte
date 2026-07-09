<script lang="ts">
	import { base } from '$app/paths';

	let open = $state(false);

	const links = [
		{ href: `${base}/docs`, label: 'Документація' },
		{ href: `${base}/spec`, label: 'Специфікація' },
		{ href: `${base}/packages`, label: 'Пакунки' },
		{ href: 'https://github.com/ChernegaSergiy/trypillia-language', label: 'GitHub' }
	];

	$effect(() => {
		document.body.style.overflow = open ? 'hidden' : '';
	});
</script>

<header>
	<div class="header-inner">
		<a href={`${base}/`} class="wordmark">Trypillia</a>
		<nav class="main-nav">
			{#each links as link}
				<a href={link.href}>{link.label}</a>
			{/each}
		</nav>
		<button class="burger" aria-label="Меню" onclick={() => (open = !open)}>
			<span></span><span></span><span></span>
		</button>
	</div>
	<div class="mobile-menu" class:active={open}>
		{#each links as link}
			<a href={link.href} onclick={() => (open = false)}>{link.label}</a>
		{/each}
	</div>
</header>
