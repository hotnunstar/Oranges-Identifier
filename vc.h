#define VC_DEBUG

typedef struct {
	unsigned char* data;
	int width, height;
	int channels;			// Binário/Cinzentos=1; RGB=3
	int levels;				// Binário=1; Cinzentos [1,255]; RGB [1,255]
	int bytesperline;		// width * channels
} IVC;

typedef struct {
	int x, y, width, height;	// Caixa Delimitadora (Bounding Box)
	int area;					// Área
	int xc, yc;					// Centro-de-massa
	int perimeter;				// Perímetro
	int label;					// Etiqueta
	float deformation;			// Deformacao
} OVC;

IVC* vc_image_new(int width, int height, int channels, int levels);											// Alocar memoria para uma imagem
IVC* vc_image_free(IVC* image);																				// Libertar memoria de uma imagem
int vc_rgb_to_hsv(IVC* src, IVC* dst);																		// Converter de RGB para HSV
int vc_hsv_segmentation(IVC* src, IVC* dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax);	// Segmentacao HSV	
int vc_binary_close(IVC* src, IVC* dst, int kDil, int kErode);												// Fecho binario
int vc_binary_dilate(IVC* src, IVC* dst, int kernel);														// Dilatacao binaria
int vc_binary_erode(IVC* src, IVC* dst, int kernel);														// Erosao binaria
int vc_binary_negative(IVC* src, IVC* dst);																	// Negativo binario
OVC* vc_binary_blob_labelling(IVC* src, IVC* dst, int* nlabels);											// Etiquetagem de blobs
int vc_binary_blob_info(IVC* src, IVC* dst, OVC* blobs, int* nblobs);										// Calcula e armazena as informacoes de cada blob
float convertPXtoMM(int pixeis);																			// Conversao de pixel para mm
int vc_clean_image(IVC* src, IVC* dst, OVC blob);															// Elimina pequenos fragmentos da imagem
int vc_remove_apple(IVC* srcdst, OVC* info1, int n, OVC* info2, int m);										// Remove objetos cujos blobs coincidam mas as areas tenham uma grande % de diferenca entre elas
int vc_final_blob_info(IVC* src, IVC* dst, OVC* blobs, int* nblobs);										// Aplicacao das bounding boxes na imagem original
int vc_final_img(IVC* src, IVC* dst, IVC* mask);															// Aplica uma mascara na imagem original
int vc_rgb_to_gray(IVC* src, IVC* dst);																		// Converter de RGB para Gray
int vc_gray_erode(IVC* src, IVC* dst, int kernel);															// Erosao gray
int vc_gray_edge_prewitt(IVC* src, IVC* dst, float th);														// Detecao de contornos pelos operadores Prewitt
int vc_calc_deformation(IVC* prewitt, OVC* info1, int counter);												// Calcula a deformacao da imagem