// =============================================================================
// Sistema de Gestion de Procesos (SGP)
//
// Descripcion:
//   Simula la administracion de procesos de un sistema operativo usando
//   exclusivamente estructuras de datos dinamicas lineales implementadas
//   desde cero: lista enlazada simple, cola de prioridad y pila.
//
// Persistencia:
//   El estado del sistema se guarda y carga desde SGP_Data.txt al iniciar
//   y al cerrar el programa.
// =============================================================================

#include <fstream> // permite leer y escribir datos en archivos externos
#include <iostream> // gestionar la entrada y salida de datos
#include <sstream> // sirve para manipular cadenas de texto como si fueran flujos de entrada/salida
#include <string> // proporciona herramientas para crear, representar y manipular cadenas de texto

using namespace std;

// Nombre del archivo donde se persiste el estado del sistema.
const char   ARCHIVO_DATOS[]   = "SGP_Data.txt";

// Etiquetas de seccion usadas al leer y escribir el archivo de datos.
const string SECCION_PROCESOS  = "---PROCESOS---";
const string SECCION_COLA      = "---COLA---";
const string SECCION_PILA      = "---PILA---";


// =============================================================================
// ESTRUCTURAS DE DATOS
// =============================================================================

// Nodo de la lista enlazada simple que representa un proceso del sistema.
// Cada nodo almacena los datos del proceso y un puntero al siguiente nodo.
struct NodoProceso {
    int          id;        // Identificador unico del proceso.
    string       nombre;    // Nombre descriptivo del proceso.
    int          prioridad; // Nivel de prioridad: 1 (minima) a 5 (maxima).
    string       estado;    // Estado actual: "Activo" por defecto.
    NodoProceso* siguiente; // Puntero al siguiente nodo de la lista.
};

// Nodo de la cola de prioridad que representa un proceso en espera de CPU.
// Los nodos se mantienen ordenados de mayor a menor prioridad. 
struct NodoCola {
    int       idProceso; // ID del proceso encolado.
    string    nombre;    // Nombre del proceso encolado.
    int       prioridad; // Prioridad que determina el orden en la cola.
    NodoCola* siguiente; // Puntero al siguiente nodo de la cola.
};

/* Nodo de la pila que representa un bloque de memoria asignado a un proceso.
La pila sigue la politica LIFO: el ultimo bloque asignado es el primero
en liberarse. */
struct NodoPila {
    int       idProceso;    // ID del proceso al que pertenece el bloque.
    int       tamanoBloque; // Tamano del bloque asignado, en kilobytes.
    NodoPila* anterior;     // Puntero al nodo que estaba en la cima antes.
};


// =============================================================================
// VALIDACION DE ENTRADAS
// =============================================================================

// Intenta convertir el texto recibido a un entero.
// Rechaza cadenas con caracteres adicionales despues del numero.
// Retorna true si la conversion fue exitosa; false en caso contrario.
bool convertirAEntero(const string& texto, int& valor) {
    stringstream flujo(texto);
    char         caracterExtra;

    // Si no se puede leer un entero, la cadena no es valida.
    if (!(flujo >> valor)) {
        return false;
    }

    // Si queda algun caracter tras el numero, la cadena no es valida.
    if (flujo >> caracterExtra) {
        return false;
    }

    return true;
}

/* Muestra el mensaje y repite la solicitud hasta que el usuario ingrese
un numero entero valido. Rechaza letras y simbolos. */
int leerEntero(const string& mensaje) {
    string linea;
    int    valor;

    while (true) {
        cout << mensaje;
        getline(cin, linea);

        if (convertirAEntero(linea, valor)) {
            return valor;
        }

        cout << "Entrada invalida. Ingrese un numero entero.\n";
    }
}

/* Solicita un entero y lo repite hasta obtener un valor estrictamente
 mayor que cero. */
int leerEnteroPositivo(const string& mensaje) {
    int valor;

    do {
        valor = leerEntero(mensaje);

        if (valor <= 0) {
            cout << "El valor debe ser mayor que cero.\n";
        }
    } while (valor <= 0);

    return valor;
}

/* Solicita un entero y lo repite hasta obtener un valor dentro del rango
de prioridad permitido: de 1 (minima) a 5 (maxima). */
int leerPrioridad(const string& mensaje) {
    int prioridad;

    do {
        prioridad = leerEntero(mensaje);

        if (prioridad < 1 || prioridad > 5) {
            cout << "La prioridad debe estar entre 1 y 5.\n";
        }
    } while (prioridad < 1 || prioridad > 5);

    return prioridad;
}

/* Recorre el texto caracter por caracter y devuelve true si encuentra
al menos un caracter que no sea espacio, tabulacion ni retorno de carro. */
bool tieneTextoVisible(const string& texto) {
    string::size_type i;

    for (i = 0; i < texto.length(); i++) {
        if (texto[i] != ' ' && texto[i] != '\t' && texto[i] != '\r') {
            return true;
        }
    }

    return false;
}

/* Muestra el mensaje y repite la solicitud hasta que el usuario ingrese
un nombre que contenga al menos un caracter visible (no vacio ni espacios). */
string leerNombre(const string& mensaje) {
    string nombre;

    do {
        cout << mensaje;
        getline(cin, nombre);

        if (!tieneTextoVisible(nombre)) {
            cout << "El nombre no puede estar vacio.\n";
        }
    } while (!tieneTextoVisible(nombre));

    return nombre;
}


// =============================================================================
// GESTOR DE PROCESOS: LISTA ENLAZADA SIMPLE
// =============================================================================

/*
La lista enlazada simple almacena todos los procesos registrados en el
sistema. Cada NodoProceso contiene los datos del proceso y un puntero
al siguiente nodo. La variable "cabeza" apunta al primer nodo de la lista;
si la lista esta vacia, cabeza vale NULL. 
*/

// Operaciones implementadas:
//   - Busqueda por ID (obtenerProcesoPorID, buscarProcesoPorID)
//   - Busqueda por nombre (buscarProcesoPorNombre)
//   - Insercion al final (insertarProceso)
//   - Eliminacion por ID (eliminarProcesoPorID)
//   - Eliminacion por nombre (eliminarProcesoPorNombre)
//   - Modificacion de prioridad (modificarPrioridad)
//   - Listado completo (listarProcesos)
// =============================================================================

/* Declaraciones anticipadas necesarias porque modificarPrioridad, definida
en esta seccion, utiliza funciones del Planificador de CPU que se
definen mas adelante en el archivo. */
bool estaEnColaCPU(NodoCola* frente, int idProceso);
bool encolarPrioridad(NodoCola*& frente, int idProceso, const string& nombre, int prioridad);

// Recorre la lista desde la cabeza buscando el nodo cuyo ID coincida.
// Devuelve la direccion del nodo encontrado, o NULL si no existe.
NodoProceso* obtenerProcesoPorID(NodoProceso* cabeza, int id) {
    NodoProceso* actual = cabeza;

    while (actual != NULL) {
        if (actual->id == id) {
            return actual;
        }
        actual = actual->siguiente;
    }

    return NULL;
}

/* Crea un nuevo nodo con los datos proporcionados y lo agrega al final
de la lista. Rechaza IDs duplicados para garantizar unicidad.
Retorna true si el proceso fue registrado; false si el ID ya existia. */
bool insertarProceso(NodoProceso*& cabeza, int id, const string& nombre, int prioridad) {
    // Verificar que no exista otro proceso con el mismo ID.
    if (obtenerProcesoPorID(cabeza, id) != NULL) {
        cout << "Ya existe un proceso registrado con el ID " << id << ".\n";
        return false;
    }

    // Crear el nuevo nodo e inicializar sus campos.
    NodoProceso* nuevo   = new NodoProceso();
    nuevo->id            = id;
    nuevo->nombre        = nombre;
    nuevo->prioridad     = prioridad;
    nuevo->estado        = "Activo";
    nuevo->siguiente     = NULL;

    // Si la lista esta vacia, el nuevo nodo pasa a ser la cabeza.
    if (cabeza == NULL) {
        cabeza = nuevo;
    } else {
        // Recorrer hasta el ultimo nodo para enlazar el nuevo al final.
        NodoProceso* actual = cabeza;

        while (actual->siguiente != NULL) {
            actual = actual->siguiente;
        }

        actual->siguiente = nuevo;
    }

    cout << "Proceso registrado correctamente.\n";
    return true;
}

/* Recorre la lista buscando el nodo con el ID indicado y lo elimina,
reconectando los nodos vecinos para mantener la lista integra. */
void eliminarProcesoPorID(NodoProceso*& cabeza, int id) {
    NodoProceso* actual   = cabeza;
    NodoProceso* anterior = NULL;

    if (cabeza == NULL) {
        cout << "La lista de procesos esta vacia.\n";
        return;
    }

    // Avanzar hasta encontrar el nodo con el ID buscado.
    while (actual != NULL && actual->id != id) {
        anterior = actual;
        actual   = actual->siguiente;
    }

    if (actual == NULL) {
        cout << "No se encontro un proceso con el ID " << id << ".\n";
        return;
    }

    // Si el nodo a eliminar es la cabeza, actualizar la cabeza.
    // Si no, enlazar el nodo anterior con el siguiente del eliminado.
    if (anterior == NULL) {
        cabeza = actual->siguiente;
    } else {
        anterior->siguiente = actual->siguiente;
    }

    delete actual;
    cout << "Proceso eliminado correctamente.\n";
}

/* Recorre la lista buscando el primer nodo cuyo nombre coincida y lo
elimina, reconectando los nodos vecinos para mantener la lista integra. */
void eliminarProcesoPorNombre(NodoProceso*& cabeza, const string& nombre) {
    NodoProceso* actual   = cabeza;
    NodoProceso* anterior = NULL;

    if (cabeza == NULL) {
        cout << "La lista de procesos esta vacia.\n";
        return;
    }

    // Avanzar hasta encontrar el primer nodo con el nombre buscado.
    while (actual != NULL && actual->nombre != nombre) {
        anterior = actual;
        actual   = actual->siguiente;
    }

    if (actual == NULL) {
        cout << "No se encontro un proceso con el nombre " << nombre << "'.\n";
        return;
    }

    // Si el nodo a eliminar es la cabeza, actualizar la cabeza.
    // Si no, enlazar el nodo anterior con el siguiente del eliminado.
    if (anterior == NULL) {
        cabeza = actual->siguiente;
    } else {
        anterior->siguiente = actual->siguiente;
    }

    delete actual;
    cout << "Proceso eliminado correctamente.\n";
}

// Muestra en pantalla todos los campos del proceso cuyo ID coincida.
void buscarProcesoPorID(NodoProceso* cabeza, int id) {
    NodoProceso* proceso = obtenerProcesoPorID(cabeza, id);

    if (proceso == NULL) {
        cout << "No se encontro un proceso con el ID " << id << ".\n";
        return;
    }

    cout << "\n--- PROCESO ENCONTRADO ---\n";
    cout << "ID: "        << proceso->id        << "\n";
    cout << "Nombre: "    << proceso->nombre    << "\n";
    cout << "Prioridad: " << proceso->prioridad << "\n";
    cout << "Estado: "    << proceso->estado    << "\n";
}

/* Recorre toda la lista y muestra los datos de cada proceso cuyo nombre
coincida con el indicado. Informa si no encuentra ninguno. */
void buscarProcesoPorNombre(NodoProceso* cabeza, const string& nombre) {
    NodoProceso* actual    = cabeza;
    bool         encontrado = false;

    while (actual != NULL) {
        if (actual->nombre == nombre) {
            cout << "\n--- PROCESO ENCONTRADO ---\n";
            cout << "ID: "        << actual->id        << "\n";
            cout << "Nombre: "    << actual->nombre    << "\n";
            cout << "Prioridad: " << actual->prioridad << "\n";
            cout << "Estado: "    << actual->estado    << "\n";
            encontrado = true;
        }

        actual = actual->siguiente;
    }

    if (!encontrado) {
        cout << "No se encontro un proceso con el nombre " << nombre << "'.\n";
    }
}

/* Cambia la prioridad del proceso en la lista enlazada. Si ese proceso
tambien esta en la cola de CPU, lo retira de su posicion actual y lo
reinserta con la nueva prioridad para mantener el orden correcto. */
void modificarPrioridad(NodoProceso* cabeza, int id,
                        int nuevaPrioridad, NodoCola*& frente) {
    NodoProceso* proceso = obtenerProcesoPorID(cabeza, id);

    if (proceso == NULL) {
        cout << "No se encontro un proceso con el ID " << id << ".\n";
        return;
    }

    // Actualizar la prioridad en la lista enlazada.
    proceso->prioridad = nuevaPrioridad;
    cout << "Prioridad del proceso ID " << id
         << " modificada a " << nuevaPrioridad << ".\n";

    // Si el proceso no esta en la cola de CPU, no hay nada mas que hacer.
    if (!estaEnColaCPU(frente, id)) {
        return;
    }

    // Buscar y retirar el nodo de la cola en su posicion actual.
    // No se usa desencolarCPU porque esa funcion solo retira el frente.
    NodoCola* actual   = frente;
    NodoCola* anterior = NULL;

    while (actual != NULL && actual->idProceso != id) {
        anterior = actual;
        actual   = actual->siguiente;
    }

    if (actual == NULL) {
        return;
    }

    // Reconectar los nodos vecinos para cerrar el hueco en la cola.
    if (anterior == NULL) {
        frente = actual->siguiente;
    } else {
        anterior->siguiente = actual->siguiente;
    }

    delete actual;

    // Reinsertar el proceso con la nueva prioridad en la posicion correcta.
    encolarPrioridad(frente, id, proceso->nombre, nuevaPrioridad);
    cout << "La posicion del proceso en la cola de CPU fue actualizada.\n";
}

// Recorre la lista completa y muestra los datos de cada proceso registrado.
void listarProcesos(NodoProceso* cabeza) {
    NodoProceso* actual = cabeza;

    if (cabeza == NULL) {
        cout << "La lista de procesos esta vacia.\n";
        return;
    }

    cout << "\n================ LISTA DE PROCESOS ================\n";

    while (actual != NULL) {
        cout << "ID: "        << actual->id;
        cout << " | Nombre: " << actual->nombre;
        cout << " | Prioridad: " << actual->prioridad;
        cout << " | Estado: " << actual->estado << "\n";

        actual = actual->siguiente;
    }

    cout << "===================================================\n";
}


// =============================================================================
// PLANIFICADOR DE CPU: COLA DE PRIORIDAD
// =============================================================================

/*
La cola de prioridad organiza los procesos que esperan tiempo de CPU.
Los nodos se mantienen ordenados de mayor a menor prioridad: el proceso
con la prioridad mas alta siempre esta al frente y es el primero en
ejecutarse. Ante prioridades iguales, el orden de llegada se conserva.
*/

/* La variable "frente" apunta al nodo con mayor prioridad. Si la cola
esta vacia, frente vale NULL.
*/

// Operaciones implementadas:
//   - Verificacion de presencia (estaEnColaCPU)
//   - Insercion ordenada (encolarPrioridad)
//   - Extraccion y ejecucion del proceso de mayor prioridad (desencolarCPU)
//   - Visualizacion del estado actual (mostrarColaCPU)
// =============================================================================

// Recorre la cola buscando un nodo cuyo ID coincida con el indicado.
// Retorna true si el proceso ya esta en la cola; false si no esta.
bool estaEnColaCPU(NodoCola* frente, int idProceso) {
    NodoCola* actual = frente;

    while (actual != NULL) {
        if (actual->idProceso == idProceso) {
            return true;
        }
        actual = actual->siguiente;
    }

    return false;
}

/* Inserta un proceso en la cola respetando el orden descendente de
prioridad. Si ya existe un nodo con la misma prioridad, el nuevo
se coloca despues para preservar el orden de llegada (FIFO entre iguales).
Rechaza el proceso si ya esta en la cola.
Retorna true si fue encolado; false si ya existia. */
bool encolarPrioridad(NodoCola*& frente, int idProceso, const string& nombre, int prioridad) {
    if (estaEnColaCPU(frente, idProceso)) {
        cout << "El proceso ID " << idProceso
             << " ya esta en la cola de CPU.\n";
        return false;
    }

    // Crear el nuevo nodo con los datos del proceso.
    NodoCola* nuevo    = new NodoCola();
    nuevo->idProceso   = idProceso;
    nuevo->nombre      = nombre;
    nuevo->prioridad   = prioridad;
    nuevo->siguiente   = NULL;

    /* Si la cola esta vacia o la nueva prioridad supera la del frente,
    insertar al inicio para que quede como el siguiente en ejecutarse. */
    if (frente == NULL || prioridad > frente->prioridad) {
        nuevo->siguiente = frente;
        frente           = nuevo;
    } else {
        // Avanzar hasta encontrar el punto de insercion correcto:
        // justo antes del primer nodo con prioridad menor a la nueva.
        NodoCola* actual = frente;

        while (actual->siguiente != NULL && actual->siguiente->prioridad >= prioridad) {
            actual = actual->siguiente;
        }

        nuevo->siguiente  = actual->siguiente;
        actual->siguiente = nuevo;
    }

    cout << "Proceso agregado a la cola de CPU.\n";
    return true;
}

/* Retira el nodo del frente de la cola (el de mayor prioridad),
muestra sus datos simulando su ejecucion y libera su memoria. */
void desencolarCPU(NodoCola*& frente) {
    NodoCola* eliminado;

    if (frente == NULL) {
        cout << "La cola de CPU esta vacia.\n";
        return;
    }

    eliminado = frente;
    frente    = frente->siguiente;

    cout << "\n--- EJECUTANDO PROCESO DE MAYOR PRIORIDAD ---\n";
    cout << "ID: "        << eliminado->idProceso << "\n";
    cout << "Nombre: "    << eliminado->nombre    << "\n";
    cout << "Prioridad: " << eliminado->prioridad << "\n";

    delete eliminado;
    cout << "Proceso ejecutado y retirado de la cola de CPU.\n";
}

/* Recorre la cola desde el frente hasta el final y muestra cada proceso
con su posicion, ID, nombre y prioridad. */
void mostrarColaCPU(NodoCola* frente) {
    NodoCola* actual   = frente;
    int       posicion = 1;

    if (frente == NULL) {
        cout << "La cola de CPU esta vacia.\n";
        return;
    }

    cout << "\n================== COLA DE CPU ==================\n";

    while (actual != NULL) {
        cout << posicion
             << ". ID: "        << actual->idProceso
             << " | Nombre: "   << actual->nombre
             << " | Prioridad: " << actual->prioridad << "\n";

        actual = actual->siguiente;
        posicion++;
    }

    cout << "=================================================\n";
}


// =============================================================================
// GESTOR DE MEMORIA: PILA
// =============================================================================
//
/* La pila administra los bloques de memoria asignados a los procesos.
Sigue la politica LIFO (ultimo en entrar, primero en salir): el ultimo
bloque asignado es el primero en liberarse, simulando el comportamiento
de una pila de llamadas de sistema operativo.
*/

/* La variable "cima" apunta al nodo en la cima de la pila. Cada nodo
apunta al nodo que estaba en la cima antes de ser insertado ("anterior").
Si la pila esta vacia, cima vale NULL.
*/

// Operaciones implementadas:
//   - Asignacion de bloque (pushMemoria)
//   - Liberacion del ultimo bloque (popMemoria)
//   - Busqueda de bloques por ID de proceso (buscarEnPila)
//   - Visualizacion de la pila completa (mostrarPilaMemoria)
// =============================================================================

/* Crea un nuevo nodo con el bloque de memoria indicado y lo coloca en la
cima de la pila, apuntando al nodo que era la cima anterior. */
void pushMemoria(NodoPila*& cima, int idProceso, int tamanoBloque) {
    NodoPila* nuevo      = new NodoPila();
    nuevo->idProceso     = idProceso;
    nuevo->tamanoBloque  = tamanoBloque;
    nuevo->anterior      = cima; // El nuevo nodo apunta al que era la cima.
    cima                 = nuevo; // El nuevo nodo pasa a ser la cima.
}

/*Retira el nodo en la cima de la pila, muestra los datos del bloque
liberado y libera su memoria dinamica. */
void popMemoria(NodoPila*& cima) {
    NodoPila* eliminado;

    if (cima == NULL) {
        cout << "La pila de memoria esta vacia.\n";
        return;
    }

    eliminado = cima;
    cima      = cima->anterior; // La cima retrocede al nodo anterior.

    cout << "Se libero el bloque de " << eliminado->tamanoBloque
         << " KB del proceso ID "     << eliminado->idProceso << ".\n";

    delete eliminado;
}

/* Recorre la pila desde la cima hasta la base buscando todos los bloques
asignados al proceso con el ID indicado. Muestra la posicion de cada
bloque contada desde la cima e informa si no hay ninguno. */
void buscarEnPila(NodoPila* cima, int idProceso) {
    NodoPila* actual    = cima;
    bool      encontrado = false;
    int       posicion   = 1;

    if (cima == NULL) {
        cout << "La pila de memoria esta vacia.\n";
        return;
    }

    cout << "\n--- BLOQUES ASIGNADOS AL PROCESO ID "
         << idProceso << " ---\n";

    /* Recorrer usando el puntero "anterior" de cada nodo para ir de la
    cima hacia la base, igual que en mostrarPilaMemoria. */
    while (actual != NULL) {
        if (actual->idProceso == idProceso) {
            cout << "Posicion desde cima: " << posicion
                 << " | Bloque: " << actual->tamanoBloque << " KB\n";
            encontrado = true;
        }

        actual = actual->anterior;
        posicion++;
    }

    if (!encontrado) {
        cout << "No se encontro ningun bloque asignado al proceso ID "
             << idProceso << ".\n";
    }
}

/* Recorre la pila desde la cima hasta la base y muestra el ID del proceso
y el tamano de cada bloque, con etiquetas CIMA y BASE para indicar
la orientacion de la estructura. */
void mostrarPilaMemoria(NodoPila* cima) {
    NodoPila* actual = cima;

    if (cima == NULL) {
        cout << "La pila de memoria esta vacia.\n";
        return;
    }

    cout << "\n=============== PILA DE MEMORIA ===============\n";
    cout << "CIMA\n";

    while (actual != NULL) {
        cout << "ID proceso: " << actual->idProceso
             << " | Bloque: "  << actual->tamanoBloque << " KB\n";

        actual = actual->anterior;
    }

    cout << "BASE\n";
    cout << "================================================\n";
}


// =============================================================================
// PERSISTENCIA EN ARCHIVO DE TEXTO
// =============================================================================
//
/* El estado completo del sistema se guarda en SGP_Data.txt con tres
secciones separadas por etiquetas: ---PROCESOS---, ---COLA--- y ---PILA---.
Cada linea dentro de una seccion representa un nodo, con sus campos
separados por el caracter '|'. */
//
/*La pila se guarda de base a cima para que al cargarla con pushMemoria
(que inserta en la cima) el orden original quede restaurado. */
// =============================================================================

/* Funcion recursiva auxiliar que llega hasta la base de la pila antes
de escribir, garantizando que el primer nodo escrito sea el de la base. */
void guardarPilaDesdeBase(ofstream& archivo, NodoPila* nodo) {
    if (nodo == NULL) {
        return;
    }

    // Descender recursivamente hasta la base antes de escribir.
    guardarPilaDesdeBase(archivo, nodo->anterior);

    // Al regresar de la recursion se escribe de base a cima.
    archivo << nodo->idProceso << "|" << nodo->tamanoBloque << "\n";
}

// Escribe el estado actual de las tres estructuras en SGP_Data.txt.
// Cada estructura queda bajo su etiqueta de seccion correspondiente.
void guardarSistema(NodoProceso* cabeza, NodoCola* frente, NodoPila* cima) {
    ofstream archivo(ARCHIVO_DATOS);

    if (!archivo) {
        cout << "No se pudo guardar el archivo SGP_Data.txt.\n";
        return;
    }

    // Guardar la lista de procesos: id|prioridad|estado|nombre
    archivo << SECCION_PROCESOS << "\n";
    while (cabeza != NULL) {
        archivo << cabeza->id       << "|"
                << cabeza->prioridad << "|"
                << cabeza->estado    << "|"
                << cabeza->nombre    << "\n";
        cabeza = cabeza->siguiente;
    }

    // Guardar la cola de CPU: idProceso|prioridad|nombre
    archivo << SECCION_COLA << "\n";
    while (frente != NULL) {
        archivo << frente->idProceso << "|"
                << frente->prioridad  << "|"
                << frente->nombre     << "\n";
        frente = frente->siguiente;
    }

    // Guardar la pila de memoria de base a cima: idProceso|tamanoBloque
    archivo << SECCION_PILA << "\n";
    guardarPilaDesdeBase(archivo, cima);

    archivo.close();
    cout << "Datos guardados en SGP_Data.txt.\n";
}

/* Inserta un proceso leido del archivo al final de la lista sin mostrar
mensajes en pantalla, ya que la carga es silenciosa. */
void agregarProcesoCargado(NodoProceso*& cabeza, int id, const string& nombre, int prioridad, const string& estado) {
    NodoProceso* nuevo   = new NodoProceso();
    nuevo->id            = id;
    nuevo->nombre        = nombre;
    nuevo->prioridad     = prioridad;
    nuevo->estado        = estado;
    nuevo->siguiente     = NULL;

    if (cabeza == NULL) {
        cabeza = nuevo;
    } else {
        NodoProceso* actual = cabeza;

        while (actual->siguiente != NULL) {
            actual = actual->siguiente;
        }

        actual->siguiente = nuevo;
    }
}

/* Inserta un proceso leido del archivo en la posicion correcta de la cola
segun su prioridad, sin mostrar mensajes en pantalla. */
void agregarColaCargada(NodoCola*& frente, int idProceso,
                        const string& nombre, int prioridad) {
    NodoCola* nuevo    = new NodoCola();
    nuevo->idProceso   = idProceso;
    nuevo->nombre      = nombre;
    nuevo->prioridad   = prioridad;
    nuevo->siguiente   = NULL;

    if (frente == NULL || prioridad > frente->prioridad) {
        nuevo->siguiente = frente;
        frente           = nuevo;
    } else {
        NodoCola* actual = frente;

        while (actual->siguiente != NULL && actual->siguiente->prioridad >= prioridad) {
            actual = actual->siguiente;
        }

        nuevo->siguiente  = actual->siguiente;
        actual->siguiente = nuevo;
    }
}

/* Lee SGP_Data.txt linea por linea e identifica la seccion activa mediante las etiquetas. 
Por cada linea valida reconstruye el nodo correspondiente y lo agrega a la estructura correcta. 
Valida cada campo antes de insertarpara descartar datos corruptos en el archivo.*/
void cargarSistema(NodoProceso*& cabeza, NodoCola*& frente, NodoPila*& cima) {
    ifstream archivo(ARCHIVO_DATOS);
    string   linea;
    string   seccion;

    if (!archivo) {
        cout << "No existe SGP_Data.txt. El sistema inicia vacio.\n";
        return;
    }

    while (getline(archivo, linea)) {
        // Detectar cambio de seccion.
        if (linea == SECCION_PROCESOS ||
            linea == SECCION_COLA     ||
            linea == SECCION_PILA) {
            seccion = linea;
            continue;
        }

        // Ignorar lineas vacias.
        if (linea.empty()) {
            continue;
        }

        if (seccion == SECCION_PROCESOS) {
            // Formato esperado: id|prioridad|estado|nombre
            string       textoID, textoPrioridad, estado, nombre;
            int          id, prioridad;
            stringstream datos(linea);

            if (getline(datos, textoID,        '|') &&
                getline(datos, textoPrioridad, '|') &&
                getline(datos, estado,         '|') &&
                getline(datos, nombre)               &&
                convertirAEntero(textoID,        id)        &&
                convertirAEntero(textoPrioridad, prioridad) &&
                id >= 1                                      &&
                prioridad >= 1 && prioridad <= 5             &&
                tieneTextoVisible(nombre)                    &&
                obtenerProcesoPorID(cabeza, id) == NULL) {
                agregarProcesoCargado(cabeza, id, nombre, prioridad, estado);
            }

        } else if (seccion == SECCION_COLA) {
            // Formato esperado: idProceso|prioridad|nombre
            string       textoID, textoPrioridad, nombre;
            int          id, prioridad;
            stringstream datos(linea);

            if (getline(datos, textoID,        '|') &&
                getline(datos, textoPrioridad, '|') &&
                getline(datos, nombre)               &&
                convertirAEntero(textoID,        id)        &&
                convertirAEntero(textoPrioridad, prioridad) &&
                id >= 1                                      &&
                prioridad >= 1 && prioridad <= 5             &&
                tieneTextoVisible(nombre)                    &&
                !estaEnColaCPU(frente, id)) {
                agregarColaCargada(frente, id, nombre, prioridad);
            }

        } else if (seccion == SECCION_PILA) {
            // Formato esperado: idProceso|tamanoBloque
            string       textoID, textoTamano;
            int          id, tamano;
            stringstream datos(linea);

            if (getline(datos, textoID,    '|') &&
                getline(datos, textoTamano)      &&
                convertirAEntero(textoID,    id)     &&
                convertirAEntero(textoTamano, tamano) &&
                id     >= 1                           &&
                tamano >= 1) {
                pushMemoria(cima, id, tamano);
            }
        }
    }

    archivo.close();
    cout << "Datos cargados desde SGP_Data.txt.\n";
}


// =============================================================================
// MENUS DE USUARIO
// =============================================================================

// Muestra el menu principal con los tres modulos del sistema.
void mostrarMenuPrincipal() {
    cout << "\n============================================\n";
    cout << "  SISTEMA DE GESTION DE PROCESOS (SGP)\n";
    cout << "============================================\n";
    cout << "1. Gestor de Procesos\n";
    cout << "2. Planificador de CPU\n";
    cout << "3. Gestor de Memoria\n";
    cout << "4. Guardar estado y salir\n";
}

// Muestra las operaciones disponibles sobre la lista enlazada de procesos.
void mostrarMenuProcesos() {
    cout << "\n---------- GESTOR DE PROCESOS ----------\n";
    cout << "1. Registrar nuevo proceso\n";
    cout << "2. Eliminar proceso por ID\n";
    cout << "3. Eliminar proceso por nombre\n";
    cout << "4. Buscar proceso por ID\n";
    cout << "5. Buscar proceso por nombre\n";
    cout << "6. Modificar prioridad\n";
    cout << "7. Listar todos los procesos\n";
    cout << "8. Volver al menu principal\n";
}

// Muestra las operaciones disponibles sobre la cola de prioridad de CPU.
void mostrarMenuCPU() {
    cout << "\n---------- PLANIFICADOR DE CPU ----------\n";
    cout << "1. Encolar proceso en espera de CPU\n";
    cout << "2. Desencolar y ejecutar proceso de mayor prioridad\n";
    cout << "3. Mostrar cola de CPU\n";
    cout << "4. Volver al menu principal\n";
}

// Muestra las operaciones disponibles sobre la pila de memoria.
void mostrarMenuMemoria() {
    cout << "\n---------- GESTOR DE MEMORIA ----------\n";
    cout << "1. Asignar bloque de memoria\n";
    cout << "2. Liberar ultimo bloque asignado\n";
    cout << "3. Buscar bloques por ID de proceso\n";
    cout << "4. Mostrar pila de memoria\n";
    cout << "5. Volver al menu principal\n";
}

// Muestra el submenu del Gestor de Procesos y atiende la opcion elegida.
// Recibe la cola de CPU porque modificar la prioridad puede reordenarla.
void menuGestorProcesos(NodoProceso*& cabeza, NodoCola*& frente) {
    int opcion;

    do {
        mostrarMenuProcesos();
        opcion = leerEntero("Seleccione una opcion: ");

        switch (opcion) {
            case 1: {
                int    id        = leerEnteroPositivo("Ingrese el ID del proceso: ");
                string nombre    = leerNombre("Ingrese el nombre del proceso: ");
                int    prioridad = leerPrioridad("Ingrese la prioridad (1 a 5): ");
                insertarProceso(cabeza, id, nombre, prioridad);
                break;
            }
            case 2: {
                int id = leerEnteroPositivo("Ingrese el ID del proceso a eliminar: ");
                eliminarProcesoPorID(cabeza, id);
                break;
            }
            case 3: {
                string nombre = leerNombre("Ingrese el nombre del proceso a eliminar: ");
                eliminarProcesoPorNombre(cabeza, nombre);
                break;
            }
            case 4: {
                int id = leerEnteroPositivo("Ingrese el ID del proceso a buscar: ");
                buscarProcesoPorID(cabeza, id);
                break;
            }
            case 5: {
                string nombre = leerNombre("Ingrese el nombre del proceso a buscar: ");
                buscarProcesoPorNombre(cabeza, nombre);
                break;
            }
            case 6: {
                int id        = leerEnteroPositivo("Ingrese el ID del proceso a modificar: ");
                int prioridad = leerPrioridad("Ingrese la nueva prioridad (1 a 5): ");
                modificarPrioridad(cabeza, id, prioridad, frente);
                break;
            }
            case 7:
                listarProcesos(cabeza);
                break;
            case 8:
                break;
            default:
                cout << "Opcion invalida.\n";
        }
    } while (opcion != 8);
}

// Muestra el submenu del Planificador de CPU y atiende la opcion elegida.
// Recibe la lista de procesos para verificar que el proceso exista antes de encolarlo.
void menuPlanificadorCPU(NodoProceso* cabeza, NodoCola*& frente) {
    int opcion;

    do {
        mostrarMenuCPU();
        opcion = leerEntero("Seleccione una opcion: ");

        switch (opcion) {
            case 1: {
                int id = leerEnteroPositivo("Ingrese el ID del proceso a encolar: ");
                NodoProceso* proceso = obtenerProcesoPorID(cabeza, id);

                if (proceso == NULL) {
                    cout << "No se puede encolar. El proceso no esta registrado.\n";
                } else {
                    encolarPrioridad(frente, proceso->id, proceso->nombre, proceso->prioridad);
                }
                break;
            }
            case 2:
                desencolarCPU(frente);
                break;
            case 3:
                mostrarColaCPU(frente);
                break;
            case 4:
                break;
            default:
                cout << "Opcion invalida.\n";
        }
    } while (opcion != 4);
}

// Muestra el submenu del Gestor de Memoria y atiende la opcion elegida.
// Recibe la lista de procesos para verificar que el proceso exista antes de asignarle un bloque de memoria.
void menuGestorMemoria(NodoProceso* cabeza, NodoPila*& cima) {
    int opcion;

    do {
        mostrarMenuMemoria();
        opcion = leerEntero("Seleccione una opcion: ");

        switch (opcion) {
            case 1: {
                int id = leerEnteroPositivo("Ingrese el ID del proceso: ");
                NodoProceso* proceso = obtenerProcesoPorID(cabeza, id);

                if (proceso == NULL) {
                    cout << "No se puede asignar memoria. El proceso no esta registrado.\n";
                } else {
                    int tamano = leerEnteroPositivo("Ingrese el tamano del bloque en KB: ");
                    pushMemoria(cima, id, tamano);
                    cout << "Bloque de " << tamano << " KB asignado al proceso ID " << id << ".\n";
                }
                break;
            }
            case 2:
                popMemoria(cima);
                break;
            case 3: {
                int id = leerEnteroPositivo("Ingrese el ID del proceso a buscar: ");
                buscarEnPila(cima, id);
                break;
            }
            case 4:
                mostrarPilaMemoria(cima);
                break;
            case 5:
                break;
            default:
                cout << "Opcion invalida.\n";
        }
    } while (opcion != 5);
}

// Recorre y libera todos los nodos de las tres estructuras para evitar
// fugas de memoria al cerrar el programa.
void liberarMemoriaSistema(NodoProceso*& procesos, NodoCola*& colaCPU, NodoPila*& pilaMemoria) {
    while (procesos != NULL) {
        NodoProceso* eliminado = procesos;
        procesos               = procesos->siguiente;
        delete eliminado;
    }

    while (colaCPU != NULL) {
        NodoCola* eliminado = colaCPU;
        colaCPU             = colaCPU->siguiente;
        delete eliminado;
    }

    while (pilaMemoria != NULL) {
        NodoPila* eliminado = pilaMemoria;
        pilaMemoria         = pilaMemoria->anterior;
        delete eliminado;
    }
}


// =============================================================================
// PROGRAMA PRINCIPAL
// =============================================================================

int main() {
    // Punteros cabeza de cada estructura; inician en NULL (estructuras vacias).
    NodoProceso* procesos    = NULL;
    NodoCola*    colaCPU     = NULL;
    NodoPila*    pilaMemoria = NULL;
    int          opcion;

    // Intentar restaurar el estado guardado en la sesion anterior.
    cargarSistema(procesos, colaCPU, pilaMemoria);

    do {
        mostrarMenuPrincipal();
        opcion = leerEntero("Seleccione una opcion: ");

        switch (opcion) {
            case 1:
                menuGestorProcesos(procesos, colaCPU);
                break;
            case 2:
                menuPlanificadorCPU(procesos, colaCPU);
                break;
            case 3:
                menuGestorMemoria(procesos, pilaMemoria);
                break;
            case 4:
                guardarSistema(procesos, colaCPU, pilaMemoria);
                cout << "Sistema finalizado correctamente.\n";
                break;
            default:
                cout << "Opcion invalida.\n";
        }
    } while (opcion != 4);

    // Liberar toda la memoria dinamica antes de cerrar.
    liberarMemoriaSistema(procesos, colaCPU, pilaMemoria);

    return 0;
}