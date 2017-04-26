#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//basic structures

struct object;
struct vtable;		// dispatch table
struct symbol;
struct closure;
typedef struct object *(*method_t)(struct closure *closure, struct object *receiver, ...);

// convenience definitions to silence compilers warnings about int/pointer
// conflicts
#define oop2i(o)	((int)((char *)(o) - (char *)0))
#define i2oop(i)	((struct object *)((char *)0 + (i)))

/*
Basic object is an abstract object. i.e

	assert(sizeof(struct object) == 0);

Objects with state variables have a non-zero size.  Every such object of a
given size is allocated along with a header containing its vtable pointer.
object pointer (oop). i.e.

	struct vtable **vtp = calloc(1, sizeof(struct vtable *)+bytes)
	return vtp+1;

Any operation on that object is done only by methods added to its vtable (or
any of its parents).

In the following structures, _vt is declared as an array of size 0 so that a
member is available but occupies no size. This gives us a convenient way of
pulling out an oop's vtable without complex cast operations.  Given a oop, its
vtable is just oop->_vt[-1]

*/

#define _VTABLE_REF struct vtable *_vt[0]

#define vtof(p)		((p)->_vt[-1])
#define oop_setvt(p, t)	((p)->_vt[-1] = (struct vtable *)(t))

struct object  {
	_VTABLE_REF;
};

struct vtable {
	_VTABLE_REF;
	char		*name;
	int             size;		// these members are accessed only by VM
	int             tally;
	struct object   **keys;
	struct object   **values;
	struct vtable  *parent;
};

struct symbol {
	_VTABLE_REF;
	char     *string;
};

struct closure {
	_VTABLE_REF;
	method_t method;
	struct object *env;	// symbol bindings
};

// global variables
struct root {
	struct vtable *atom_vt;
	struct vtable *proto_vt;
	struct vtable *object_vt;
	struct vtable *symbol_vt;
	struct vtable *closure_vt;

	struct symbol *lookup;		// lookup method body
	struct symbol *add_method;	// add method to a receiver
	struct symbol *allocate;	// allocate a new object
	struct symbol *delegate;	// create a child vtable
	struct symbol *newp;		// allocate a new instance of x bytes
	struct symbol *print;		// print contents
	struct symbol *length;		// size of contents

	struct object *proto;
	struct object *object;
	struct object *symbol;
} Roots;

/*
 The Object_vt is setup by adding lookup and add_method manually
 to get the send primitive going and then allocate and delegate
 are by sending addMethod message.
*/

#define s_vtlookup	Roots.lookup
#define s_vtadd_method	Roots.add_method
#define s_vtallocate	Roots.allocate
#define s_vtdelegate	Roots.delegate
#define s_newp		Roots.newp
#define s_length	Roots.length
#define s_print		Roots.print

#define Atom_vt		Roots.atom_vt
#define Proto_vt	Roots.proto_vt
#define Symbol_vt	Roots.symbol_vt
#define Object_vt	Roots.object_vt
#define Closure_vt	Roots.closure_vt

#define Proto		Roots.proto
#define Object		Roots.object
#define Symbol		Roots.symbol

/*
 * The methods below are required to bootstrap the operation of a dispatch
 * table. They are the only ones which take a vtable pointer instead of an
 * object pointer. Once vtable is primed to create vtables, create object
 * instances, add methods and lookup symbols, then the send mechanism becomes
 * operational.
*/
static struct object *vt_lookup(struct closure *cls,     struct vtable *self,  struct symbol *key);
static struct object *vt_add_method(struct closure *cls, struct vtable *self,  struct symbol *selector, method_t method);
static struct object *vt_allocate(struct closure *cls,   struct vtable *self,  size_t bytes);
static struct vtable *vt_delegate(struct closure *cls,   struct vtable *parent,char *name);

static struct closure *vt_bind(struct object *rcv,       struct symbol *selector);

static struct object *obj_selfie(struct closure *cls, struct object *self)
{
	return self;
}

static void print_sym(struct symbol *s) {printf("%p (#%s) ", s, s ? s->string : "(nil)");}

static void dump_sym(struct symbol *s) {print_sym(s); printf("\n");}

static void dump_vt(struct vtable *vt)
{
	int i;

	if (!vt) {
		printf("vt nil\n");
		return;
	}
	printf("vt %s_vt %p parent %p (%s_vt) size %d tally %d\n", vt->name, vt, vt->parent,
					vt->parent ? vt->parent->name : "(nil)", vt->size, vt->tally);
	for (i = 0; i < vt->tally; i++) {
		printf("    %d key ", i);
		print_sym((struct symbol *)(vt->keys[i]));
		printf("value %p\n", vt->values[i]);
	}
}

static void print_obj(char *name, struct object *p)
{
	struct vtable *vt = vtof(p);

	printf("obj %s %p vt %s_vt %p\n", name, p, vt ? vt->name : "0", vt);
}

#define dump_obj(obj)	print_obj(#obj, obj)

// basic allocators consist of allocators for vtable and object An object can
// be of arbitrary size (including 0), but it always starts with a pointer to
// its vtable. This pointer is hidden from other methods

struct object *vt_allocate(struct closure *cls, struct vtable *self, size_t bytes)
{
	struct vtable **vtp;
	struct object *oop;

	assert(bytes < 4096);	// sanity check
	vtp = (struct vtable **)calloc(1, sizeof(struct vtable *) + bytes);
	oop = (struct object *)(vtp+1);
	oop_setvt(oop, self);
	return oop;
}

static struct vtable *vt_delegate(struct closure *cls, struct vtable *parent, char *name)
{
	struct vtable *vt = (typeof(vt))vt_allocate(0,
					parent ? vtof(parent) : 0, sizeof(*vt));

	vt->name    = strdup(name);
	vt->size    = 2; // start with two slots
	vt->tally   = 0;
	vt->keys    = (typeof(vt->keys))calloc(vt->size, sizeof(vt->keys[0]));
	vt->values  = (typeof(vt->values))calloc(vt->size, sizeof(vt->values[0]));
	vt->parent  = parent;
	// child and parent share the same vt
	assert(vtof(vt) == (parent ? vtof(parent) : 0));
	return vt;
}

#define send(receiver, selector, ...) ({			\
      struct object  *r = (struct object *)(receiver);	\
      struct closure *c = vt_bind(r, (selector));	\
      c ? c->method(c, r, ##__VA_ARGS__) : 0;	\
    })

static struct closure *vt_bind(struct object *rcv, struct symbol *selector)
{
	static int nesting = 0;
	struct closure *c;
	struct vtable *rvt = vtof(rcv);

	assert(rvt);
	struct vtable *super = rvt->parent;

	if ((selector == s_vtlookup) && (rcv == (struct object *)Proto_vt))
		return (struct closure *)vt_lookup(0, rvt, selector);
	nesting++;
	assert(nesting < 100);	// catch infinite recursion
	c = (struct closure *)send(rvt, s_vtlookup, selector);
	nesting--;
	return c;
}

static struct object *vt_lookup(struct closure *cls, struct vtable *self, struct symbol *key)
{
	int i;
	struct closure *mnu_cls = 0;

	for (i = 0;  i < self->tally;  ++i)
		if (key == (struct symbol *)(self->keys[i]))
			return self->values[i];
	if (self->parent) {
		assert(self != self->parent);
		return send(self->parent, s_vtlookup, key);
	}
	printf("unknown selector "); print_sym(key);
	dump_vt(self);
	return 0;
}

static struct object *cls_alloc(method_t method, struct object *env)
{
	struct closure *cls;

	cls = (typeof(cls))vt_allocate(0, Closure_vt, sizeof(*cls));
	cls->method  = method;
	cls->env     = env;
	return (struct object *)cls;
}

static struct object *vt_add_method(struct closure *cls, struct vtable *vt,
				struct symbol *selector, method_t method)
{
	struct object *key = (struct object *)selector;
	int i;

	for (i = 0;  i < vt->tally;  ++i)
		if (key == vt->keys[i]) {
			((struct closure *)vt->values[i])->method = method;
			return key;
		}
	if (vt->tally == vt->size) {
		vt->size  *= 2;
		vt->keys   = (typeof(vt->keys))realloc(vt->keys,    vt->size*sizeof(vt->keys[0]));
		vt->values = (typeof(vt->values))realloc(vt->values,vt->size*sizeof(vt->values[0]));
	}
	vt->keys  [vt->tally  ] = key;
	vt->values[vt->tally++] = cls_alloc(method, 0);
	return key;
}

static struct symbol *sym_alloc(char *string)
{
	struct symbol *symbol;
	char *symstr;
	size_t sbytes = strlen(string)+1;

	symbol = (struct symbol *)vt_allocate(0, Symbol_vt, sizeof(struct symbol)+sbytes);
	symstr = (char *)symbol+sizeof(struct symbol);
	strncpy(symstr, string, sbytes);
	symbol->string = symstr;
	return symbol;
}

static struct symbol *atom(char *string)
{
	struct symbol *symbol;
	int i;

	for (i = 0;  i < Atom_vt->tally;  ++i) {
		symbol = (struct symbol *)Atom_vt->keys[i];
      		if (!strcmp(string, symbol->string))
			return symbol;
	}
	symbol = sym_alloc(string);
	vt_add_method(0, Atom_vt, symbol, (method_t)obj_selfie);
	return symbol;
}

// Symbol>>#new:
static struct object *symbol_newp(struct closure *cls, struct object *obj, char *str)
{
	assert(Symbol_vt);
	return (struct object *)atom(str);
}

// Symbol>>#print
static struct object *symbol_print(struct closure *cls, struct object *self)
{
	printf("#%s", ((struct symbol *)self)->string);
	return self;
}

// Symbol>>#length
static struct object *symbol_length(struct closure *cls, struct object *self)
{
	size_t len = strlen(((struct symbol *)self)->string);
	return i2oop(len);
}


void init_ovm(void)
{
	struct object *selector;

	Atom_vt     = vt_delegate(0, 0, "Atom");

	// object vt does not exist yet, so proto vt has no parent
	Proto_vt    = vt_delegate(0, 0, "Proto");
	oop_setvt(Proto_vt,  Proto_vt); 

	Object_vt   = vt_delegate(0, 0, "Object");
	oop_setvt(Object_vt, Proto_vt);

	// now connect object vt as a parent for proto vt
	Proto_vt->parent = Object_vt;

	Symbol_vt   = vt_delegate(0, Object_vt, "Symbol");
	Closure_vt  = vt_delegate(0, Object_vt, "Closure");

	s_vtlookup     = atom("lookup");
	s_vtadd_method = atom("add_method");
	s_vtallocate   = atom("allocate");
	s_vtdelegate   = atom("delegate");
	s_newp         = atom("new:");
	s_print        = atom("print");
	s_length       = atom("length");

	vt_add_method(0, Proto_vt, s_vtlookup,        (method_t)vt_lookup);
	vt_add_method(0, Proto_vt, s_vtadd_method,    (method_t)vt_add_method);
	send(Proto_vt,  s_vtadd_method, s_vtallocate, (method_t)vt_allocate);
	send(Proto_vt,  s_vtadd_method, s_vtdelegate, (method_t)vt_delegate);

	Proto  = vt_allocate(0, Proto_vt,  0);
	Object = vt_allocate(0, Object_vt, 0);
	Symbol = vt_allocate(0, Symbol_vt, 0);

	assert(vtof(Proto) == Proto_vt);
	assert(vtof(Object) == Object_vt);
	assert(vtof(Symbol) == Symbol_vt);

	// dump_vt(Proto_vt); dump_obj(Proto);
	// dump_vt(Object_vt); dump_obj(Object);

	send(vtof(Symbol), s_vtadd_method, s_newp,       (method_t)symbol_newp);
	send(vtof(Symbol), s_vtadd_method, s_print,      (method_t)symbol_print);
	send(vtof(Symbol), s_vtadd_method, s_length,     (method_t)symbol_length);

	// dump_vt(Symbol_vt); dump_obj(Symbol);

	// printf("Object Machine ready\n");
}
