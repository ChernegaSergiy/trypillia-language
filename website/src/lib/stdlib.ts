// Documentation is generated directly from the compiler's own doc source
// (src/lsp/native_docs.json) so it never drifts from the real standard library.
// No copy is kept: this import resolves the live file at build time.
import raw from '../../../src/lsp/native_docs.json';

export interface ParamDoc {
	name: string;
	doc: string;
}

export interface FuncDoc {
	key: string;
	module: string;
	name: string;
	signature: string;
	returns: string | null;
	doc: string;
	params: ParamDoc[];
}

export interface ModuleDoc {
	name: string;
	description: string;
	functions: FuncDoc[];
}

interface RawParam {
	label: string;
	doc?: string;
}

interface RawEntry {
	signature: string;
	doc: string;
	params?: RawParam[];
}

const moduleInfo: Record<string, string> = {
	Array: 'Методи списків (масивів) та ітератори.',
	Crypto: 'Криптографічні хеш-функції (SHA-256, SHA-3, BLAKE3).',
	Error: 'Робота з помилками та їхньою інформацією.',
	File: 'Читання та запис файлів на диску.',
	Http: 'Виконання HTTP-запитів.',
	Json: 'Серіалізація та десеріалізація JSON.',
	List: 'Операції над списками.',
	Map: 'Асоціативні словники (ключ-значення).',
	Math: 'Математичні функції та константи.',
	Net: 'Мережеві утиліти та адреси.',
	OS: 'Інформація про операційну систему та середовище.',
	Random: 'Генерація випадкових чисел.',
	Regex: 'Регулярні вирази.',
	Result: 'Тип Result для обробки помилок без винятків.',
	Socket: 'Низькорівневі TCP-сокети.',
	String: 'Операції над рядками.',
	Terminal: 'Робота з терміналом та кольоровим виводом.',
	Time: 'Час, таймери та дати.',
	WeakRef: 'Слабкі посилання на об’єкти.',
	WebSocket: 'Клієнт WebSocket.',
	WebSocketServer: 'Сервер WebSocket.',
	Worker: 'Ізольовані потоки виконання.',
	input: 'Глобальні функції введення.',
	print: 'Глобальні функції виведення.'
};

function cleanName(label: string): string {
	return label.split(':')[0].trim();
}

function build(source: Record<string, RawEntry>): ModuleDoc[] {
	const byModule: Record<string, FuncDoc[]> = {};

	for (const [key, entry] of Object.entries(source)) {
		const dot = key.indexOf('.');
		const module = dot === -1 ? key : key.slice(0, dot);
		const name = dot === -1 ? key : key.slice(dot + 1);

		const params: ParamDoc[] = (entry.params ?? []).map((p) => ({
			name: cleanName(p.label ?? ''),
			doc: p.doc ?? ''
		}));

		const returnsMatch = entry.signature.match(/->\s*(.+)$/);

		const func: FuncDoc = {
			key,
			module,
			name,
			signature: `${key}(${params.map((p) => p.name).join(', ')})`,
			returns: returnsMatch ? returnsMatch[1].trim() : null,
			doc: entry.doc,
			params
		};

		(byModule[module] ??= []).push(func);
	}

	return Object.keys(byModule)
		.sort((a, b) => a.localeCompare(b))
		.map((m) => ({
			name: m,
			description: moduleInfo[m] ?? '',
			functions: byModule[m].sort((a, b) => a.name.localeCompare(b.name))
		}));
}

export const modules: ModuleDoc[] = build(raw as Record<string, RawEntry>);

export const moduleMap: Record<string, ModuleDoc> = Object.fromEntries(
	modules.map((m) => [m.name, m])
);

export function getModule(name: string): ModuleDoc | undefined {
	return moduleMap[name];
}

export function getFunction(module: string, func: string): FuncDoc | undefined {
	return moduleMap[module]?.functions.find((f) => f.name === func);
}
