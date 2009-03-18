// Compile with:
// 	g++ verify.cc -o verify -lcrypto

#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/objects.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/pem.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#define	PUBLIC_CERTIFICATE_DATA "-----BEGIN PUBLIC KEY-----\nMIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQCg6Xnvoa8vsGURrDzW9stKxi9U\nuKXf4aUqFFrcxO6So9XKpygV4oN3nwBip3rGhIg4jbNbQrhAeicQhfyvATYenj6W\nBLh4X3GbUD/LTYqLNY4qQGsdt/BpO0smp4DPIVpvAPSOeY6424+en4RRnUrsNPJu\nuShWNvQTd0XRYlj4ywIDAQAB\n-----END PUBLIC KEY-----\n"

using namespace std;

string slurp(string fn) {
	ifstream ifs(fn.c_str());
	
	if(!ifs.is_open()){
		throw string("could not open " + fn);
	}

    stringstream sstr;
    sstr << ifs.rdbuf();
    return sstr.str();
}

// From http://www.google.com/codesearch/p?hl=en#Q5tR35FJDOM/libopkele-0.2.1/lib/util.cc&q=decode_base64%20const%20string%20data%20lang:c%2B%2B

vector<unsigned char> decode_base64(const string& data) {
	vector<unsigned char> rv;
    BIO *b64 = 0, *bmem = 0;
    rv.clear();
	
    try {
        bmem = BIO_new_mem_buf((void*)data.data(),data.size());
        if(!bmem)
            throw "failed to allocate in base64 decoder";
        b64 = BIO_new(BIO_f_base64());
        if(!b64)
            throw "failed to initialize in base64 decoder";
        BIO_push(b64,bmem);
        unsigned char tmp[512];
        size_t rb = 0;
        while((rb=BIO_read(b64,tmp,sizeof(tmp)))>0){
            rv.insert(rv.end(),tmp,&tmp[rb]);
		}
        BIO_free_all(b64);
    }catch(...) {
        if(b64) BIO_free_all(b64);
        throw;
    }
	
	return rv;
}

EVP_PKEY* get_public_key(){
	string public_key = string(PUBLIC_CERTIFICATE_DATA); //slurp("public_key.pem");
	BIO *mbio = BIO_new_mem_buf((void *)public_key.c_str(), public_key.size());
	return PEM_read_bio_PUBKEY(mbio, NULL, NULL, NULL);
}

bool verify(){
    EVP_MD_CTX ctx;

	ERR_load_crypto_strings();

	// Load the public key
	EVP_PKEY* public_key;
	cout << "Loading public key.. ";
	try{
		public_key = get_public_key();
	}catch(string err){
		cout << "failure (" << err << ")\n";
		return false;
	}
	cout << "ok\n";
	
	// Get the hardware info file
	string hwinfo;
	cout << "Loading hardware info.. ";
	try{
		hwinfo = slurp("license.txt");
	}catch(string err){
		cout << "failure (" << err << ")\n";
		return false;
	}
	cout << "ok\n";

	// Load the signature and decode from base64
	vector<unsigned char> signature;
	cout << "Loading signature.. ";
	try{
		signature = decode_base64(slurp("license.sig"));
	}catch(string err){
		cout << "failure (" << err << ")\n";
		return false;
	}
	cout << "ok\n";

	cout << "Verifying signature.. ";

	// Create a char buffer of the signature to pass to openssl
	unsigned char *buffer = new unsigned char[signature.size()];
	int i;

	for (i=0; i < signature.size(); i++){
		buffer[i] = signature[i];
	}
	
	// Initialize openssl and pass in the data
    EVP_VerifyInit(&ctx, EVP_sha1());
    EVP_VerifyUpdate(&ctx, hwinfo.data(), hwinfo.size());

	// Call the actual verify function and get result
    int result = EVP_VerifyFinal(&ctx, buffer, signature.size(), public_key);

    if (result == 1){
		cout << "ok\n";
		return true;
    }else if (result == 0){
		cout << "failure (incorrect signature)\n";
		return false;
    }else{
		cout << "failure (other error)\n";
		return false;
	}
}

int main(int argc, char **argv){
	if (verify()){
		cout << "Running in full version\n";
	}else{
		cout << "Running in demo mode\n";
	}
	
	return 0;

}