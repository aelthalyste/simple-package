#define PACKAGE_IMPLEMENTATION
#include "package.hpp"

struct example_struct_to_serialize {
	int a,b,c;
	char some_string[26];
	double f, s, k;
};

void create_package(const char *name) {
	Package_Creator package;

	init_package_creator(&package);

	int val = 25;
	long long int li = 256;
	const char *mystr = "haha this is my data";
	
	unsigned char mystack[10];
	for(int i=0;i<sizeof(mystack);++i){
		mystack[i]=i;
	}

	example_struct_to_serialize ex_struct;// you should initialize your struct! its just silly example here.

	// well, entry data can be anything. Im just being lazy here and passing primitive types. But you can literally put anything here and it would work.
	package.add_entry("integer value!", &val  , sizeof(val)     , PACKAGE_ENTRY_NO_TAG);
	package.add_entry("long long int!", &li   , sizeof(li)      , PACKAGE_ENTRY_NO_TAG);
	package.add_entry("long long int!", mystr , strlen(mystr)   , PACKAGE_ENTRY_NO_TAG);
	package.add_entry("my array!"     , mystack, sizeof(mystack), PACKAGE_ENTRY_NO_TAG);
	package.add_entry("my custom struct", &ex_struct, sizeof(ex_struct), PACKAGE_ENTRY_NO_TAG);
	// either build to file or memory
	
	// build to file
	{
		package.build_package_to_file(name); 
	}

	// build to memory
	{
		uint64_t bfcap = package.calculate_size_needed_for_package();
		void *package_in_memory = malloc(bfcap);
		package.build_package_to_memory(package_in_memory, bfcap);
		// do something with package_in_memory
		free(package_in_memory);
	}


	free_package_creator(&package);
	
}

void list_package_contents(const char *fn) {
	// blindly iterating entries in package
	Package_Reader package;
	if (init_package_reader_from_file(&package, fn)) {
		
		for(void *entry = package.iterate_entries(NULL); entry; entry=package.iterate_entries(entry)) {
			char *entry_name;
			uint64_t entry_offset;
			uint64_t entry_size;

			package.resolve_entry(entry, &entry_name, &entry_offset, &entry_size);
			fprintf(stdout, "- name : %s\n", entry_name);
			fprintf(stdout, "---- offset : %llu\n", entry_offset);
			fprintf(stdout, "---- size   : %llu\n", entry_size);
			fprintf(stdout, "\n");
		}
		
		free_package_reader(&package);
	}		
}


int main() {

	create_package("simple_package");	
	list_package_contents("simple_package");

	Package_Reader package;
	if (init_package_reader_from_file(&package, "simple_package")) {
		uint64_t size;
		void *n = NULL;
	
	    // extracting entries by querying by name. get_entry returns pointer to entry data and it's second argument sets entry size, if one exists.
		n = package.get_entry("integer value!", &size);
		fprintf(stdout, "data : %p, size : %llu\n", n, size);

		n = package.get_entry("ttt", &size);
		fprintf(stdout, "data : %p, size : %llu\n", n, size);

		n = package.get_entry("my array!", &size);
		fprintf(stdout, "data : %p, size : %llu\n", n, size);
		
		n = package.get_entry("my struct", &size);
		fprintf(stdout, "data : %p, size : %llu\n", n, size);
	}
	else {
		fprintf(stdout, "unable to open file\n");
	}

	return 0;
}
