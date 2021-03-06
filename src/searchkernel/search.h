struct hashtable;

void searchSimple (int *TeffArrayElementer, struct iindexFormat **TeffArray,int *TotaltTreff,
                query_array *queryParsed, struct queryTimeFormat *queryTime,
                struct subnamesFormat subnames[], int nrOfSubnames,int languageFilterNr,
                int languageFilterAsNr[], char orderby[],
                struct filtersFormat *filters,
		struct filteronFormat *filteron,
		query_array *search_user_as_query,
		int ranking, struct hashtable **crc32maphash, struct duplicate_docids **dups,
		char *search_user, int cmc_port, int anonymous,
		container ***groups_per_usersystem, int **usersystem_per_subname // For repositoryaccess()
		);

#ifdef BLACK_BOX
char* searchFilterCount(int *TeffArrayElementer, 
			struct iindexFormat *TeffArray, 
			struct filtersFormat *filters,
			struct subnamesFormat subnames[], 
			int nrOfSubnames,
			struct filteronFormat *filteron,
			int dates[],
			struct queryTimeFormat *queryTime,
			struct fte_data *getfiletypep,
			struct adf_data *attrdescrp,
			attr_conf *showattrp,
			query_array *qa,
			int outformat
		);

void searchFilterInit(struct filtersFormat *filters,int dates[]);
#endif
