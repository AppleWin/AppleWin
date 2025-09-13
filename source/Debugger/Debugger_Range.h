RangeType_t Range_GetPrefix(const int iArg, AddressPrefix_t* pAP);
bool Range_GetAllPrefixes(int& iArg, const int nArg, int& dArgPrefix, AddressPrefix_t* pAP, bool slotCheck = true);
RangeType_t Range_Get( WORD & nAddress1_, WORD &nAddress2_, const int iArg = 1 );
bool Range_CalcEndLen( const RangeType_t eRange
		, const WORD & nAddress1, const WORD & nAddress2
		, WORD & nAddressEnd_, int & nAddressLen_ );
