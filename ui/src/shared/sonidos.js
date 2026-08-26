/**
 * La lógica pura del navegador de sonidos: buscar sin acentos ni mayúsculas
 * y ordenar con los favoritos siempre arriba. Vive aparte del DOM para que
 * los tests la ejerciten sin arrancar nada.
 */

/** Minúsculas y sin diacríticos: "Épica" y "epica" son lo mismo buscando. */
export function normalizar(texto) {
  return String(texto).toLowerCase().normalize('NFD').replace(/[\u0300-\u036f]/g, '');
}

/**
 * Filtra por nombre (subcadena, normalizada) y ordena: favoritos primero,
 * después alfabético. No muta la lista de entrada.
 */
export function filtrarSonidos(archivos, busqueda, favoritos = new Set()) {
  const aguja = normalizar(busqueda || '').trim();
  const lista = (archivos || []).filter((a) => !aguja || normalizar(a.nombre).includes(aguja));
  const peso = (a) => (favoritos.has(a.ruta) ? 0 : 1);
  return lista.sort((x, y) => peso(x) - peso(y) || x.nombre.localeCompare(y.nombre, 'es'));
}
