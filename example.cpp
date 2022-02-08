#define PACKAGE_IMPLEMENTATION
#include "package.cpp"

void create_package(const char *name) {
	Package_Creator package;

	init_package_creator(&package);

	int val = 25;
	long long int li = 256;
	int i = 0;
	const char *mystr = "haha this is my data";
	
	unsigned char mystack[10];
	for(int i=0;i<sizeof(mystack);++i){
		mystack[i]=i;
	}

	package.add_entry("integer value!", &val  , sizeof(val)     , 0, ++i);
	package.add_entry("long long int!", &li   , sizeof(li)      , 0, ++i);
	package.add_entry("long long int!", mystr , strlen(mystr)   , 0, ++i);
	package.add_entry("my array!"     , mystack, sizeof(mystack), 0, ++i);

	package.build_package_to_file(name);
	free_package_creator(&package);
}

void list_package_contents(const char *fn) {
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
	
		n = package.get_entry("integer value!", &size);
		fprintf(stdout, "data : %p, size : %llu\n", n, size);

		n = package.get_entry("ttt", &size);
		fprintf(stdout, "data : %p, size : %llu\n", n, size);

		n = package.get_entry("my array!", &size);
		fprintf(stdout, "data : %p, size : %llu\n", n, size);
	}
	else {
		fprintf(stdout, "unable to open file\n");
	}

	return 0;
}
