Because Blowfish only supports encrypting 8 bytes of data at once, it is necessary to pad data (with `NUL`s) that is not a multiple of 8 bytes. When receiving encrypted data from FLHook, you should check for null characters and remove them from the decrypted string. When sending encrypted data to FLHook, you should pad the string with null characters if necessary. If the Blowfish implementation you are using specifically encrypts and decrypts strings, this may already be handled by it.