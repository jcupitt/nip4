// shim for filemodel

#define IS_FILEMODEL(X) (true)
#define FILEMODEL(X) ((Filemodel *) X)

void filemodel_inter_saveas(GtkWindow *, Filemodel *);
void filemodel_inter_save(GtkWindow *, Filemodel *);
void filemodel_inter_savenclose(GtkWindow *, Filemodel *);
