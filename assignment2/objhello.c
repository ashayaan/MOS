#include "ovm.c"

// Begin String definitions
struct mystring { _VTABLE_REF; int length; char *chars; }; 
typedef struct mystring *HString;

static struct vtable *String_vt;
static struct object *String;

// String>>#new:
static struct object *String_newp(struct closure *cls, struct object *self, char *chars)
{
	HString clone = (HString)send(vtof(self), s_vtallocate, sizeof(struct mystring));

	clone->length = strlen(chars);
	clone->chars  = strdup(chars);
	return (struct object *)clone;
}

// String>>#length
static struct object *String_length(struct closure *cls, HString self) { return i2oop(self->length); }

// String>>#print
static struct object * String_print(struct closure * cls, HString self)
{
	int i;
	for (i = 0; i < self->length; i++)
		putchar(self->chars[i]);
	return (struct object * )self;
}

//Append code
static struct object * String_append(struct closure * cls, HString self1,HString self2){
	char *conc  = (char *)malloc(sizeof(char)*(self1->length+self2->length));
	for(int i=0;i<self1->length;i+=1){
		conc[i]=self1->chars[i];
	}

	int start = self1->length;

	for(int i=0;i<self2->length;i+=1){
		conc[start++]=self2->chars[i];
	}

	struct object *temp  = send(String, s_newp, conc);
	
	return (struct object * )temp;
}



// ------------------------ Begin Array definitions
struct array { _VTABLE_REF; int length; struct object **contents; };
typedef struct array *HArray;

static struct vtable *Array_vt;
static struct object *Array;

//Array>>#new:
static struct object *Array_newp(struct closure *cls, struct object *self, int length)
{
	HArray clone = (HArray)send(vtof(self), s_vtallocate, sizeof(struct array));

	clone->length   = length;
	clone->contents = (struct object **)calloc(clone->length, sizeof(struct object *));
	assert(clone->contents);
	return (struct object *)clone;
}

//Array>>#length
static struct object *Array_length(struct closure * cls, HArray self) { return i2oop(self->length); }

//Array>>#at:
static struct object *Array_at(struct closure *cls, HArray self, int ix)
{
	// index starts at 1
	if (0 < ix && ix <= self->length)
		return self->contents[ix-1];
	return 0;
}

//Array>>#at:put:
static struct object *Array_atput(struct closure *cls, HArray self, int ix, struct object *rval)
{
	// index starts at 1
	if (0 < ix && ix <= self->length)
		return self->contents[ix-1] = rval;
	return rval;
}

static struct symbol *s_at;
static struct symbol *s_atput;
static struct symbol *s_append;
static struct symbol *s_sizeInMemory;

static struct object * String_sizeInMemory(struct closure * cls, struct object *self){
	int temp=0;
	
	if(vtof(self) == String_vt){
		temp=(oop2i(send(self,s_length))+sizeof(int)+sizeof(vtof(self)));
	}

	else if(vtof(self) == Array_vt){
		int l = oop2i(send(self,s_length));
		temp=(l*sizeof(struct object *)+sizeof(int)+sizeof(vtof(self)));
	}
	else 
		return 0;
	temp = temp + (4- (temp%4));
	return i2oop(temp);

}



int main(int argc, char *argv[])
{
	init_ovm();

	s_at    = (typeof(s_at))   send(Symbol, s_newp, "at:");
	s_atput = (typeof(s_atput))send(Symbol, s_newp, "at:put:");
	s_append = (typeof(s_append))send(Symbol, s_newp, "append");
 	s_sizeInMemory = (typeof(s_sizeInMemory))send(Symbol, s_newp, "sizeInMemory");
	
	printf("Testing String\n");
	String_vt = (typeof(String_vt))send(Object_vt, s_vtdelegate, "String");
	String    = (typeof(String))send((struct object *)String_vt, s_vtallocate, 0);

	assert(vtof(String) == String_vt);

	send(String_vt, s_vtadd_method, s_newp,   (method_t)String_newp);
	send(String_vt, s_vtadd_method, s_length, (method_t)String_length);
	send(String_vt, s_vtadd_method, s_print,  (method_t)String_print);
	send(String_vt, s_vtadd_method, s_append,  (method_t)String_append);
	send(vtof(Object),s_vtadd_method,s_sizeInMemory,(method_t)String_sizeInMemory);
	

	
	struct object *h     = send(String, s_newp, "hello");
	struct object *sp    = send(String, s_newp, " ");
	struct object *w     = send(String, s_newp, "world");
	struct object *nl    = send(String, s_newp, "\n");

	printf("hello sizeInMemory %d\n", oop2i(send(h, s_sizeInMemory)));

	printf("\nTesting append\n");
	struct object *fname     = send(String, s_newp, "Ahmad ");
	struct object *lname     = send(String, s_newp, "Shayaan\n");
	struct object *cname     = send(fname,s_append,lname);
	send(cname,s_print);
	
	printf("\nTesting append\n");
	struct object *fname2     = send(String, s_newp, "Vineet ");
	struct object *lname2     = send(String, s_newp, "Reddy \n");
	struct object *cname2     = send(fname2,s_append,lname2);
	send(cname2,s_print);


	printf("\nTesting part 3\n");
	struct object *temp = send(send(h,s_append,sp),s_append,w);
	struct object *result = send(temp,s_append,nl);
	send(result,s_print);
	printf("\n");

	printf("Testing Array\n");
	Array_vt  = (typeof(Array_vt)) send(Object_vt, s_vtdelegate, "Array");
	Array     = (typeof(Array))    send((struct object *)Array_vt,  s_vtallocate, 0);

	assert(vtof(Array) == Array_vt);

	send(Array_vt,  s_vtadd_method, s_newp,   (method_t)Array_newp);
	send(Array_vt,  s_vtadd_method, s_length, (method_t)Array_length);
	send(Array_vt,  s_vtadd_method, s_at,     (method_t)Array_at);
	send(Array_vt,  s_vtadd_method, s_atput,  (method_t)Array_atput);

	struct object *line = send(Array, s_newp, 4);

	// printf("Testing Array\n");
	send(line, s_atput, 1, h);
	send(line, s_atput, 2, sp);
	send(line, s_atput, 3, w); 
	send(line, s_atput, 4, nl);

	printf("Array sizeInMemory %d\n", oop2i(send(line, s_sizeInMemory)));
	printf("\n");

	// printf("Array sizeInMemory %d\n", oop2i(send("hello", s_sizeInMemory)));

	
	return 0;
}

