# nfc_creditcard_reader

Based on the PoC readnfccc by Renaud Lifchitz (renaud.lifchitz@bt.com)

License: distributed under GPL version 3 (http://www.gnu.org/licenses/gpl.html)

* Introduction:
    "Quick and dirty" proof-of-concept
    Open source tool developped and showed for Hackito Ergo Sum 2012 - "Hacking the NFC credit cards for fun and debit ;)"
    Reads NFC credit card personal data (gender, first name, last name, PAN, expiration date, transaction history...) 

* Requirements:
    libnfc (>= 1.6.0-rc1) and a suitable NFC reader (http://www.libnfc.org/documentation/hardware/compatibility)

* Compilation: 
    $ gcc nfc_creditcard_reader.c -lnfc -o nfc_creditcard_reader
