import { error } from '@sveltejs/kit';
import { modules, getFunction } from '$lib/stdlib';

export const prerender = true;

export function entries() {
	const result: { module: string; function: string }[] = [];
	for (const m of modules) {
		for (const f of m.functions) {
			result.push({ module: m.name, function: f.name });
		}
	}
	return result;
}

export function load({ params }) {
	const func = getFunction(params.module, params.function);
	if (!func) {
		throw error(404, `Функцію "${params.module}.${params.function}" не знайдено`);
	}
	return { func };
}
