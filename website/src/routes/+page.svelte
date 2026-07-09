<script lang="ts">
	import { base } from '$app/paths';
	import CodeBlock from '$lib/components/CodeBlock.svelte';
</script>

<svelte:head>
	<title>Trypillia Programming Language</title>
</svelte:head>

<section class="hero">
	<div class="hero-inner">
		<div class="hero-text">
			<h1 class="hero-title">Мова програмування Trypillia</h1>
			<p class="hero-desc">
				Динамічно типізована мова загального призначення для надійного ПЗ зі
				зрозумілим синтаксисом та передбачуваною продуктивністю.
			</p>
			<div class="hero-actions">
				<a href={`${base}/docs`} class="btn btn-primary">Почати роботу</a>
				<a href="https://github.com/ChernegaSergiy/trypillia-language/releases" class="btn">Завантажити v1.0</a>
			</div>
		</div>
		<CodeBlock
			code={`// Обчислення числа Фібоначчі
fn fibonacci(n) {
  if (n <= 1) {
    return n;
  }
  return fibonacci(n - 1) + fibonacci(n - 2);
}

let result = fibonacci(10);
print("Результат: {result}");`}
		/>
	</div>
</section>

<div class="container">
	<main>
		<section id="docs">
			<h2>Швидкий старт</h2>
			<p>Офіційні посібники та довідники зі стандартної бібліотеки для швидкого занурення.</p>
			<ul class="list-group">
				<li>
					<span>Основи синтаксису</span>
					<a href={`${base}/docs/language`}>Читати →</a>
				</li>
				<li>
					<span>Стандартна бібліотека</span>
					<a href={`${base}/docs/stdlib`}>Читати →</a>
				</li>
			</ul>
		</section>

		<section id="pkg">
			<h2>Пакетний менеджер</h2>
			<p>Створюйте проєкти за допомогою TrypPM.</p>
			<div class="cli-box">
				<div><span>$</span> tryppm init my_project</div>
				<div><span>$</span> cd my_project</div>
				<div><span>$</span> tryppm run</div>
			</div>
		</section>
	</main>

	<aside>
		<div class="sidebar-section">
			<h3>Встановлення</h3>
			<p>Linux / macOS:</p>
			<div class="code-block" style="border: none; padding: 0;">
				<code style="border: none; background: transparent; padding: 0.65rem;">curl -sSL https://tryp.pp.ua/install.sh | sh</code>
			</div>
			<p style="margin-top: 1rem;">Windows (PowerShell):</p>
			<div class="code-block" style="border: none; padding: 0;">
				<code style="border: none; background: transparent; padding: 0.65rem;">irm https://tryp.pp.ua/install.ps1 | iex</code>
			</div>
		</div>

		<div class="sidebar-section">
			<h3>Реліз v1.0-draft</h3>
			<dl class="dl-grid">
				<dt>Версія</dt>
				<dd>1.0.0-beta.2</dd>
				<dt>Дата</dt>
				<dd>24 Жовтня, 2026</dd>
				<dt>Ліцензія</dt>
				<dd>CSSM-ULv2</dd>
			</dl>
		</div>
	</aside>
</div>
