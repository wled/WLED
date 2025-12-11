const fs = require('fs');
const path = require('path');

// Diccionario técnico español-inglés
const dictionary = {
  'WLED': 'WLED', 'LED': 'LED', 'WiFi': 'WiFi', 'API': 'API', 'JSON': 'JSON',
  'WebSocket': 'WebSocket', 'MQTT': 'MQTT', 'UDP': 'UDP', 'HTTP': 'HTTP',
  'E1.31': 'E1.31', 'NeoPixel': 'NeoPixel', 'WS2812B': 'WS2812B', 'SK6812': 'SK6812',
  'SPI': 'SPI', 'I2C': 'I2C', 'UART': 'UART', 'GPIO': 'GPIO', 'EEPROM': 'EEPROM',
  'main': 'principal', 'setup': 'configuración', 'loop': 'bucle',
  'initialize': 'inicializar', 'update': 'actualizar', 'render': 'renderizar',
  'draw': 'dibujar', 'paint': 'pintar', 'clear': 'limpiar', 'reset': 'restablecer',
  'enable': 'habilitar', 'disable': 'deshabilitar', 'start': 'iniciar',
  'stop': 'detener', 'pause': 'pausar', 'resume': 'reanudar', 'save': 'guardar',
  'load': 'cargar', 'delete': 'eliminar', 'create': 'crear', 'destroy': 'destruir',
  'check': 'verificar', 'validate': 'validar', 'convert': 'convertir',
  'parse': 'analizar', 'format': 'formato', 'configure': 'configurar',
  'connect': 'conectar', 'disconnect': 'desconectar', 'send': 'enviar',
  'receive': 'recibir', 'read': 'leer', 'write': 'escribir', 'append': 'añadir',
  'insert': 'insertar', 'remove': 'eliminar', 'replace': 'reemplazar',
  'search': 'buscar', 'find': 'encontrar', 'match': 'coincidir', 'compare': 'comparar',
  'index': 'índice', 'offset': 'desplazamiento', 'position': 'posición',
  'size': 'tamaño', 'length': 'longitud', 'count': 'conteo', 'value': 'valor',
  'variable': 'variable', 'constant': 'constante', 'parameter': 'parámetro',
  'argument': 'argumento', 'return': 'retorno', 'result': 'resultado',
  'error': 'error', 'warning': 'advertencia', 'info': 'información',
  'debug': 'depuración', 'trace': 'rastreo', 'log': 'registro', 'print': 'imprimir',
  'output': 'salida', 'input': 'entrada', 'buffer': 'búfer', 'array': 'matriz',
  'list': 'lista', 'queue': 'cola', 'stack': 'pila', 'tree': 'árbol',
  'node': 'nodo', 'link': 'enlace', 'connection': 'conexión', 'network': 'red',
  'server': 'servidor', 'client': 'cliente', 'request': 'solicitud',
  'response': 'respuesta', 'status': 'estado', 'state': 'estado', 'code': 'código',
  'message': 'mensaje', 'data': 'datos', 'payload': 'carga útil', 'header': 'encabezado',
  'body': 'cuerpo', 'content': 'contenido', 'text': 'texto', 'html': 'HTML',
  'xml': 'XML', 'css': 'CSS', 'javascript': 'JavaScript', 'function': 'función',
  'method': 'método', 'procedure': 'procedimiento', 'routine': 'rutina',
  'handler': 'manejador', 'listener': 'escuchador', 'callback': 'devolución de llamada',
  'event': 'evento', 'trigger': 'disparador', 'action': 'acción', 'effect': 'efecto',
  'animation': 'animación', 'transition': 'transición', 'color': 'color',
  'brightness': 'brillo', 'intensity': 'intensidad', 'speed': 'velocidad',
  'duration': 'duración', 'delay': 'retraso', 'timeout': 'tiempo de espera',
  'interval': 'intervalo', 'frequency': 'frecuencia', 'pixel': 'píxel',
  'strip': 'tira', 'segment': 'segmento', 'range': 'rango', 'limit': 'límite',
  'threshold': 'umbral', 'tolerance': 'tolerancia', 'precision': 'precisión',
  'performance': 'rendimiento', 'optimization': 'optimización', 'memory': 'memoria',
  'storage': 'almacenamiento', 'cache': 'caché', 'heap': 'montón', 'thread': 'hilo',
  'process': 'proceso', 'task': 'tarea', 'job': 'trabajo', 'scheduler': 'planificador',
  'timer': 'temporizador', 'interrupt': 'interrupción', 'signal': 'señal',
  'exception': 'excepción', 'fault': 'fallo', 'crash': 'bloqueo',
  'deadlock': 'bloqueo mutuo', 'race': 'condición de carrera', 'condition': 'condición',
  'mutex': 'mutex', 'semaphore': 'semáforo', 'lock': 'bloqueo', 'unlock': 'desbloqueo',
  'atomic': 'atómico', 'volatile': 'volátil', 'synchronous': 'síncrono',
  'asynchronous': 'asíncrono', 'blocking': 'bloqueante', 'non-blocking': 'no bloqueante',
  'promise': 'promesa', 'future': 'futuro', 'await': 'esperar', 'async': 'asíncrono',
  'generator': 'generador', 'iterator': 'iterador', 'enumeration': 'enumeración',
  'flag': 'bandera', 'bit': 'bit', 'byte': 'byte', 'word': 'palabra',
  'integer': 'entero', 'float': 'flotante', 'double': 'doble', 'string': 'cadena',
  'character': 'carácter', 'boolean': 'booleano', 'true': 'verdadero',
  'false': 'falso', 'null': 'nulo', 'undefined': 'indefinido', 'overflow': 'desbordamiento',
  'underflow': 'subdesbordamiento', 'truncate': 'truncar', 'round': 'redondear',
  'ceil': 'techo', 'floor': 'piso', 'absolute': 'absoluto', 'sign': 'signo',
  'magnitude': 'magnitud', 'scale': 'escala', 'normalize': 'normalizar',
  'interpolate': 'interpolar', 'extrapolate': 'extrapolar', 'blend': 'mezcla',
  'combine': 'combinar', 'merge': 'fusionar', 'split': 'dividir', 'chunk': 'fragmento',
  'partition': 'partición', 'distribute': 'distribuir', 'balance': 'equilibrio',
  'load': 'carga', 'capacity': 'capacidad', 'utilization': 'utilización',
  'efficiency': 'eficiencia', 'latency': 'latencia', 'throughput': 'rendimiento',
  'bandwidth': 'ancho de banda', 'jitter': 'inestabilidad', 'skew': 'sesgo',
  'bias': 'sesgo', 'variance': 'varianza', 'deviation': 'desviación',
  'standard': 'estándar', 'specification': 'especificación', 'requirement': 'requisito',
  'constraint': 'restricción', 'dependency': 'dependencia', 'relationship': 'relación',
  'association': 'asociación', 'aggregation': 'agregación', 'composition': 'composición',
  'inheritance': 'herencia', 'polymorphism': 'polimorfismo', 'abstraction': 'abstracción',
  'encapsulation': 'encapsulamiento', 'interface': 'interfaz', 'implementation': 'implementación',
  'contract': 'contrato', 'protocol': 'protocolo', 'sdk': 'SDK',
  'framework': 'marco de trabajo', 'library': 'biblioteca', 'module': 'módulo',
  'package': 'paquete', 'component': 'componente', 'subsystem': 'subsistema',
  'system': 'sistema', 'platform': 'plataforma', 'application': 'aplicación',
  'service': 'servicio', 'middleware': 'middleware', 'layer': 'capa',
  'tier': 'nivel', 'level': 'nivel', 'hierarchy': 'jerarquía', 'topology': 'topología',
  'architecture': 'arquitectura', 'design': 'diseño', 'pattern': 'patrón',
  'template': 'plantilla', 'strategy': 'estrategia', 'algorithm': 'algoritmo',
  'heuristic': 'heurística', 'estimation': 'estimación', 'calculation': 'cálculo',
  'computation': 'computación', 'evaluation': 'evaluación', 'assessment': 'evaluación',
  'analysis': 'análisis', 'synthesis': 'síntesis', 'modeling': 'modelado',
  'simulation': 'simulación', 'emulation': 'emulación', 'virtualization': 'virtualización',
  'deployment': 'implementación', 'installation': 'instalación',
  'customization': 'personalización', 'extension': 'extensión', 'plugin': 'complemento',
  'addon': 'complemento', 'usermod': 'usermod', 'firmware': 'firmware',
  'bootloader': 'bootloader', 'kernel': 'kernel', 'driver': 'controlador',
  'device': 'dispositivo', 'hardware': 'hardware', 'software': 'software',
  'port': 'puerto', 'socket': 'socket', 'endpoint': 'extremo',
  'gateway': 'puerta de enlace', 'proxy': 'proxy', 'router': 'enrutador',
  'switch': 'conmutador', 'firewall': 'cortafuegos', 'encryption': 'cifrado',
  'decryption': 'descifrado', 'hash': 'hash', 'digest': 'resumen', 'signature': 'firma',
  'certificate': 'certificado', 'authentication': 'autenticación',
  'authorization': 'autorización', 'access': 'acceso', 'permission': 'permiso',
  'privilege': 'privilegio', 'role': 'rol', 'user': 'usuario', 'admin': 'administrador',
  'owner': 'propietario', 'group': 'grupo', 'domain': 'dominio', 'realm': 'reino',
  'zone': 'zona', 'context': 'contexto', 'scope': 'alcance', 'namespace': 'espacio de nombres',
  'version': 'versión', 'release': 'lanzamiento', 'build': 'compilación',
  'patch': 'parche', 'upgrade': 'mejora', 'downgrade': 'degradación',
  'migration': 'migración', 'rollback': 'reversión', 'commit': 'confirmación',
  'revert': 'revertir', 'merge': 'fusión', 'branch': 'rama', 'tag': 'etiqueta',
  'rebase': 'cambiar base', 'squash': 'comprimir', 'stash': 'almacenar',
  'staging': 'área de preparación', 'working': 'funcionamiento', 'repository': 'repositorio',
  'fork': 'bifurcación', 'clone': 'clon', 'pull': 'extraer', 'push': 'enviar',
  'fetch': 'obtener', 'sync': 'sincronizar', 'conflict': 'conflicto',
  'resolution': 'resolución', 'diff': 'diferencia', 'blame': 'culpa',
  'history': 'historial', 'stats': 'estadísticas', 'metric': 'métrica',
  'benchmark': 'punto de referencia', 'profile': 'perfil', 'breakpoint': 'punto de ruptura',
  'watchpoint': 'punto de observación', 'step': 'paso', 'continue': 'continuar',
  'break': 'ruptura', 'exit': 'salida', 'quit': 'salir', 'abort': 'abortar',
  'retry': 'reintentar', 'skip': 'omitir', 'ignore': 'ignorar', 'suppress': 'suprimir',
  'filter': 'filtro', 'regex': 'expresión regular', 'wildcard': 'comodín',
  'glob': 'glob', 'path': 'ruta', 'directory': 'directorio', 'folder': 'carpeta',
  'file': 'archivo', 'attribute': 'atributo', 'property': 'propiedad',
  'field': 'campo', 'member': 'miembro', 'static': 'estático', 'instance': 'instancia',
  'class': 'clase', 'struct': 'estructura', 'union': 'unión', 'typedef': 'definición de tipo',
  'macro': 'macro', 'define': 'definir', 'ifdef': 'si está definido',
  'ifndef': 'si no está definido', 'endif': 'fin si', 'include': 'incluir',
  'import': 'importar', 'export': 'exportar', 'using': 'usando', 'extern': 'externo',
  'const': 'constante', 'mutable': 'mutable', 'inline': 'en línea',
  'virtual': 'virtual', 'override': 'anular', 'final': 'final', 'operator': 'operador',
  'overload': 'sobrecarga', 'typename': 'nombre de tipo', 'specialization': 'especialización',
  'instantiation': 'instanciación', 'generic': 'genérico', 'type': 'tipo',
  'cast': 'conversión', 'coercion': 'coerción', 'promotion': 'promoción',
  'demotion': 'degradación', 'widening': 'ampliación', 'narrowing': 'reducción',
};

function translateComment(text) {
  if (!text) return text;
  let result = text;
  
  Object.entries(dictionary).forEach(([en, es]) => {
    const regex = new RegExp(`\\b${en}\\b`, 'gi');
    result = result.replace(regex, (match) => {
      if (match === match.toUpperCase() && match.length > 1) {
        return es.toUpperCase();
      }
      if (match[0] === match[0].toUpperCase()) {
        return es.charAt(0).toUpperCase() + es.slice(1);
      }
      return es;
    });
  });
  
  return result;
}

function processFile(filePath) {
  const ext = path.extname(filePath).toLowerCase();
  const allowedExts = ['.cpp', '.h', '.js', '.html', '.css'];
  
  if (!allowedExts.includes(ext)) {
    return { success: false, translated: false };
  }

  try {
    let content = fs.readFileSync(filePath, 'utf-8');
    const original = content;

    // Comentarios de bloque /* */
    content = content.replace(/\/\*[\s\S]*?\*\//g, (match) => {
      const inner = match.slice(2, -2);
      const translated = translateComment(inner);
      return '/*' + translated + '*/';
    });

    // Comentarios de línea //
    content = content.replace(/^(\s*)\/\/(.*)$/gm, (match, indent, comment) => {
      return indent + '//' + translateComment(comment);
    });

    // Comentarios HTML <!-- -->
    content = content.replace(/<!--[\s\S]*?-->/g, (match) => {
      const inner = match.slice(4, -3);
      const translated = translateComment(inner);
      return '<!--' + translated + '-->';
    });

    const translated = content !== original;
    if (translated) {
      fs.writeFileSync(filePath, content, 'utf-8');
      return { success: true, translated: true, path: filePath };
    }

    return { success: true, translated: false };
  } catch (error) {
    return { success: false, translated: false };
  }
}

function scanDirectory(dir) {
  const files = [];
  const excluded = ['node_modules', '.git', '.next', 'dist', 'build', 'coverage', '.vscode'];
  
  try {
    const items = fs.readdirSync(dir);
    items.forEach(item => {
      const filePath = path.join(dir, item);
      try {
        const stat = fs.statSync(filePath);
        
        if (stat.isDirectory()) {
          if (!excluded.some(e => filePath.includes(e))) {
            files.push(...scanDirectory(filePath));
          }
        } else {
          files.push(filePath);
        }
      } catch (e) {
        // Ignorar
      }
    });
  } catch (error) {
    // Ignorar
  }
  
  return files;
}

// Ejecutar
const files = scanDirectory('/workspaces/WLED');
let translated = 0;

files.forEach((file) => {
  const result = processFile(file);
  if (result.translated) {
    translated++;
    console.log(`✓ ${file}`);
  }
});

console.log(`\n✅ Traducción completada: ${translated} archivos`);
