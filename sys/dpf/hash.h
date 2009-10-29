

/*
 * From CLR: use upper bits from multiply; on 32 bit machines,
 * mask is unnecessary.
 *
 * XXX: Should be selecting between hash functions.
 */
static inline unsigned hash(Ht ht, uint32 val) {
        unsigned mask, prec_bits;

        prec_bits = 32 - ht->log2_sz;
        mask = ht->htsz - 1;
        return ( (val * hashmult) >> prec_bits ) & mask;
}


