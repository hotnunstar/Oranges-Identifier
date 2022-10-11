// Desabilita (no MSVC++) warnings de funções não seguras (fopen, sscanf, etc...)
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <malloc.h>
#include <math.h>
#include "vc.h"

#define MAX(r,g,b) (r>b?(r>g?r:g):(b>g?b:g))
#define MIN(r,g,b) (r<b?(r<g?r:g):(b<g?b:g))
#define MMAX(a, b) (a > b ? a : b)
#define MMIN(a, b) (a < b ? a : b)
#define CONV_RANGE(value, range, new_range) (value / range) * new_range
#define HSV_2_RGB(value) CONV_RANGE(value, 360, 255)

// Alocar memoria para uma imagem
IVC* vc_image_new(int width, int height, int channels, int levels)
{
	IVC* image = (IVC*)malloc(sizeof(IVC));

	if (image == NULL) return NULL;
	if ((levels <= 0) || (levels > 255)) return NULL;

	image->width = width;
	image->height = height;
	image->channels = channels;
	image->levels = levels;
	image->bytesperline = image->width * image->channels;
	image->data = (unsigned char*)malloc(image->width * image->height * image->channels * sizeof(char));

	if (image->data == NULL)
	{
		return vc_image_free(image);
	}

	return image;
}

// Libertar memoria de uma imagem
IVC* vc_image_free(IVC* image)
{
	if (image != NULL)
	{
		if (image->data != NULL)
		{
			free(image->data);
			image->data = NULL;
		}

		free(image);
		image = NULL;
	}

	return image;
}

// Converter de RGB para HSV
int vc_rgb_to_hsv(IVC* src, IVC* dst) {
	unsigned char* data_src = (unsigned char*)src->data;
	int bytesperline_src = src->bytesperline;
	int channels_src = src->channels;

	unsigned char* data_dst = (unsigned char*)dst->data;
	int bytesperline_dst = dst->bytesperline;
	int channels_dst = dst->channels;
	long int pos_src, pos_dst;
	float r, g, b, sat, hue, max, min;

	if ((src->width <= 0) || (src->height <= 0) || src == NULL) return 0;
	if ((src->height != dst->height) || (src->width != dst->width)) return 0;
	if ((channels_dst != 3) || (channels_src != 3)) return 0;

	for (int i = 0; i < src->height; i++)
		for (int j = 0; j < src->width; j++)
		{
			pos_src = i * bytesperline_src + j * channels_src;
			pos_dst = i * bytesperline_dst + j * channels_dst;

			max = MAX(data_src[pos_src], data_src[pos_src + 1], data_src[pos_src + 2]);
			min = MIN(data_src[pos_src], data_src[pos_src + 1], data_src[pos_src + 2]);

			if (max == 0) {
				sat = 0;
				hue = 0;
			}
			else {
				sat = (float)(max - min) / max;

				if (sat == 0) hue = 0;
				else {
					r = (float)data_src[pos_src];
					g = (float)data_src[pos_src + 1];
					b = (float)data_src[pos_src + 2];

					if (max == r && g >= b) hue = (float)60 * (g - b) / (max - min);
					else if (max == r && g < b) hue = (float)360 + 60 * (g - b) / (max - min);
					else if (max == g) hue = (float)120 + 60 * (g - b) / (max - min);
					else if (max == b) hue = (float)240 + 60 * (g - b) / (max - min);

					hue = hue / 360 * 255;
				}
			}


			data_dst[pos_dst] = hue;
			data_dst[pos_dst + 1] = sat * 255;
			data_dst[pos_dst + 2] = max;
		}

	return 1;
}

// Segmentacao HSV
int vc_hsv_segmentation(IVC* src, IVC* dst, int hmin, int hmax, int smin, int smax, int vmin, int vmax)
{
	unsigned char* data_src = (unsigned char*)src->data, * data_dst = (unsigned char*)dst->data;
	int width_src = src->width, width_dst = dst->width;
	int height_src = src->height, height_dst = dst->height;
	int channels_src = src->channels, channel_dst = dst->channels;
	int bytesperline_src = width_src * channels_src, bytesperline_dst = width_dst * channel_dst;
	int x, y;
	float hue, sat, value;

	if ((width_src <= 0) || (height_src <= 0) || (data_src == NULL))
		return 0;
	if (channels_src != 3 || channel_dst != 1)
		return 0;

	if ((width_src != width_dst) || (height_src != height_dst))
		return 0;

	hmin = HSV_2_RGB((float)hmin);
	hmax = HSV_2_RGB((float)hmax);

	smin = CONV_RANGE((float)smin, 100, 255);
	smax = CONV_RANGE((float)smax, 100, 255);

	vmin = CONV_RANGE((float)vmin, 100, 255);
	vmax = CONV_RANGE((float)vmax, 100, 255);

	for (y = 0; y < height_src; y++)
	{
		for (x = 0; x < width_src; x++)
		{
			int pos_src = y * bytesperline_src + x * channels_src;
			int pos_dst = y * bytesperline_dst + x * channel_dst;

			hue = (float)data_src[pos_src];
			sat = (float)data_src[pos_src + 1];
			value = (float)data_src[pos_src + 2];

			data_dst[pos_dst] = ((hue >= hmin && hue <= hmax) &&
				(sat >= smin && sat <= smax) &&
				(value >= vmin && value <= vmax))
				? 255
				: 0;
		}
	}

	return 1;
}

// Fecho binario
int vc_binary_close(IVC* src, IVC* dst, int kDil, int kErode)
{
	IVC* aux;
	aux = vc_image_new(src->width, src->height, src->channels, src->levels);

	vc_binary_dilate(src, aux, kDil);
	vc_binary_erode(aux, dst, kErode);

	vc_image_free(aux);

	return 1;
}

// Dilatacao binaria
int vc_binary_dilate(IVC* src, IVC* dst, int kernel)
{
	unsigned char* data_src = (unsigned char*)src->data;
	int bytesperline_src = src->bytesperline;
	int channels_src = src->channels;
	int width = src->width;
	int height = src->height;
	int val = kernel / 2;
	int state;

	unsigned char* data_dst = (unsigned char*)dst->data;
	int bytesperline_dst = dst->bytesperline;
	int channels_dst = dst->channels;
	long int pos_src, pos_dst, pos_pix;

	// VerificaCAo de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height)) return 0;
	if ((src->channels != 1) || (dst->channels != 1)) return 0;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pos_src = i * bytesperline_src + j * channels_src;
			pos_dst = i * bytesperline_dst + j * channels_dst;

			if (data_src[pos_src] == 0)
			{
				state = 0;
				for (int x = i - val; x <= i + val; x++)
				{
					for (int y = j - val; y <= j + val; y++)
					{
						if (x >= 0 && x <= height && y >= 0 && y <= width)
						{
							pos_pix = x * bytesperline_src + y * channels_src;
							if (pos_pix != pos_src && data_src[pos_pix] == 255)
								state = 1;
						}
					}
				}
			}
			if (state == 1)
				data_dst[pos_dst] = 255;
			else
				data_dst[pos_dst] = data_src[pos_src];
		}
	}
	return 1;
}

// Erosao binaria
int vc_binary_erode(IVC* src, IVC* dst, int kernel)
{
	unsigned char* data_src = (unsigned char*)src->data;
	int bytesperline_src = src->bytesperline;
	int channels_src = src->channels;
	int width = src->width;
	int height = src->height;
	int val = kernel / 2;
	int state;

	unsigned char* data_dst = (unsigned char*)dst->data;
	int bytesperline_dst = dst->bytesperline;
	int channels_dst = dst->channels;
	long int pos_src, pos_dst, pos_pix;

	// VerificaCAo de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height)) return 0;
	if ((src->channels != 1) || (dst->channels != 1)) return 0;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pos_src = i * bytesperline_src + j * channels_src;
			pos_dst = i * bytesperline_dst + j * channels_dst;

			if (data_src[pos_src] == 255)
			{
				state = 0;
				for (int x = i - val; x <= i + val; x++)
				{
					for (int y = j - val; y <= j + val; y++)
					{
						if (x >= 0 && x <= height && y >= 0 && y <= width)
						{
							pos_pix = x * bytesperline_src + y * channels_src;
							if (pos_pix != pos_src && data_src[pos_pix] == 0)
								state = 1;
						}
					}
				}
			}
			if (state == 1)
				data_dst[pos_dst] = 0;
			else
				data_dst[pos_dst] = data_src[pos_src];
		}
	}
	return 1;
}

// Negativo binario
int vc_binary_negative(IVC* src, IVC* dst)
{
	unsigned char* data_src = (unsigned char*)src->data;
	int bytesperline_src = src->bytesperline;
	int channels_src = src->channels;
	int width = src->width;
	int height = src->height;

	unsigned char* data_dst = (unsigned char*)dst->data;
	int bytesperline_dst = dst->bytesperline;
	int channels_dst = dst->channels;
	long int pos_src, pos_dst;

	// VerificaCAo de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height)) return 0;
	if ((src->channels != 1) || (dst->channels != 1)) return 0;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pos_src = i * bytesperline_src + j * channels_src;
			pos_dst = i * bytesperline_dst + j * channels_dst;

			if (data_src[pos_src] == 0) data_dst[pos_dst] = 255;
			if (data_src[pos_src] == 255) data_dst[pos_dst] = 0;
		}
	}
	return 1;
}

// Etiquetagem de blobs
// src		: Imagem binária de entrada
// dst		: Imagem grayscale (irá conter as etiquetas)
// nlabels	: Endereço de memOria de uma variável, onde será armazenado o número de etiquetas encontradas.
// OVC*		: Retorna um array de estruturas de blobs (objetos), com respetivas etiquetas. E necessário libertar posteriormente esta memaria.
OVC* vc_binary_blob_labelling(IVC* src, IVC* dst, int* nlabels)
{
	unsigned char* datasrc = (unsigned char*)src->data;
	unsigned char* datadst = (unsigned char*)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, a, b;
	long int i, size;
	long int posX, posA, posB, posC, posD;
	int labeltable[256] = { 0 };
	int labelarea[256] = { 0 };
	int label = 1; // Etiqueta inicial.
	int num, tmplabel;
	OVC* blobs; // Apontador para array de blobs (objetos) que ser� retornado desta fun��o.

	// Verifica��o de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return NULL;
	if (channels != 1) return NULL;

	// Copia dados da imagem binaria para imagem grayscale
	memcpy(datadst, datasrc, bytesperline * height);

	// Todos os pixeis de plano de fundo devem obrigatoriamente ter valor 0
	// Todos os pixeis de primeiro plano devem obrigatoriamente ter valor 255
	// Serão atribuidas etiquetas no intervalo [1,254]
	// Este algoritmo está assim limitado a 254 labels
	for (i = 0, size = bytesperline * height; i < size; i++)
	{
		if (datadst[i] != 0) datadst[i] = 255;
	}

	// Limpa os rebordos da imagem binaria
	for (y = 0; y < height; y++)
	{
		datadst[y * bytesperline + 0 * channels] = 0;
		datadst[y * bytesperline + (width - 1) * channels] = 0;
	}
	for (x = 0; x < width; x++)
	{
		datadst[0 * bytesperline + x * channels] = 0;
		datadst[(height - 1) * bytesperline + x * channels] = 0;
	}

	// Efetua a etiquetagem
	for (y = 1; y < height - 1; y++)
	{
		for (x = 1; x < width - 1; x++)
		{
			// Kernel:
			// A B C
			// D X
			posA = (y - 1) * bytesperline + (x - 1) * channels; // A
			posB = (y - 1) * bytesperline + x * channels; // B
			posC = (y - 1) * bytesperline + (x + 1) * channels; // C
			posD = y * bytesperline + (x - 1) * channels; // D
			posX = y * bytesperline + x * channels; // X

			// Se o pixel foi marcado
			if (datadst[posX] != 0)
			{
				if ((datadst[posA] == 0) && (datadst[posB] == 0) && (datadst[posC] == 0) && (datadst[posD] == 0))
				{
					datadst[posX] = label;
					labeltable[label] = label;
					label++;
				}
				else
				{
					num = 255;

					// Se A esta marcado
					if (datadst[posA] != 0) num = labeltable[datadst[posA]];
					// Se B esta marcado, e � menor que a etiqueta "num"
					if ((datadst[posB] != 0) && (labeltable[datadst[posB]] < num)) num = labeltable[datadst[posB]];
					// Se C esta marcado, e � menor que a etiqueta "num"
					if ((datadst[posC] != 0) && (labeltable[datadst[posC]] < num)) num = labeltable[datadst[posC]];
					// Se D esta marcado, e � menor que a etiqueta "num"
					if ((datadst[posD] != 0) && (labeltable[datadst[posD]] < num)) num = labeltable[datadst[posD]];

					// Atribui a etiqueta ao pixel
					datadst[posX] = num;
					labeltable[num] = num;

					// Atualiza a tabela de etiquetas
					if (datadst[posA] != 0)
					{
						if (labeltable[datadst[posA]] != num)
						{
							for (tmplabel = labeltable[datadst[posA]], a = 1; a < label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posB] != 0)
					{
						if (labeltable[datadst[posB]] != num)
						{
							for (tmplabel = labeltable[datadst[posB]], a = 1; a < label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posC] != 0)
					{
						if (labeltable[datadst[posC]] != num)
						{
							for (tmplabel = labeltable[datadst[posC]], a = 1; a < label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
					if (datadst[posD] != 0)
					{
						if (labeltable[datadst[posD]] != num)
						{
							for (tmplabel = labeltable[datadst[posC]], a = 1; a < label; a++)
							{
								if (labeltable[a] == tmplabel)
								{
									labeltable[a] = num;
								}
							}
						}
					}
				}
			}
		}
	}

	// Volta a etiquetar a imagem
	for (y = 1; y < height - 1; y++)
	{
		for (x = 1; x < width - 1; x++)
		{
			posX = y * bytesperline + x * channels; // X

			if (datadst[posX] != 0)
			{
				datadst[posX] = labeltable[datadst[posX]];
			}
		}
	}

	//printf("\nMax Label = %d\n", label);

	// Contagem do número de blobs
	// Passo 1: Eliminar, da tabela, etiquetas repetidas
	for (a = 1; a < label - 1; a++)
	{
		for (b = a + 1; b < label; b++)
		{
			if (labeltable[a] == labeltable[b]) labeltable[b] = 0;
		}
	}
	// Passo 2: Conta etiquetas e organiza a tabela de etiquetas, para que nao hajam valores vazios (zero) entre etiquetas
	*nlabels = 0;
	for (a = 1; a < label; a++)
	{
		if (labeltable[a] != 0)
		{
			labeltable[*nlabels] = labeltable[a]; // Organiza tabela de etiquetas
			(*nlabels)++; // Conta etiquetas
		}
	}

	// Se nao ha blobs
	if (*nlabels == 0) return NULL;

	// Cria lista de blobs (objetos) e preenche a etiqueta
	blobs = (OVC*)calloc((*nlabels), sizeof(OVC));
	if (blobs != NULL)
	{
		for (a = 0; a < (*nlabels); a++) blobs[a].label = labeltable[a];
	}
	else return NULL;

	return blobs;
}

// Calcula e armazena as informacoes de cada blob
int vc_binary_blob_info(IVC* src, IVC* dst, OVC* blobs, int* nblobs)
{
	unsigned char* datasrc = (unsigned char*)src->data;
	unsigned char* datadst = (unsigned char*)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, i;
	long int pos;
	int xmin, ymin, xmax, ymax;
	long int sumx, sumy;

	//Verificacao de erros
	if (src == NULL)
	{
		printf("Error -> vc_binary_blob_info():\n\tImage is empy!\n");
		getchar();
		return 0;
	}

	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
	{
		printf("Error -> vc_binary_blob_info():\n\tImage Dimensions or data are missing!\n");
		getchar();
		return 0;
	}

	if (channels != 1)
	{
		printf("Error -> vc_binary_blob_info():\n\tImages with incorrect format!\n");
		getchar();
		return 0;
	}

	// Conta area de cada blob
	for (i = 0; i < *nblobs; i++)
	{
		xmin = width - 1;
		ymin = height - 1;
		xmax = 0;
		ymax = 0;

		sumx = 0;
		sumy = 0;

		blobs[i].area = 0;

		for (y = 1; y < height - 1; y++)
		{
			for (x = 1; x < width - 1; x++)
			{
				pos = y * bytesperline + x * channels;

				if (datasrc[pos] == blobs[i].label)
				{
					// area
					blobs[i].area++;

					// Centro de Gravidade
					sumx += x;
					sumy += y;

					// Bounding Box
					if (xmin > x)
						xmin = x;
					if (ymin > y)
						ymin = y;
					if (xmax < x)
						xmax = x;
					if (ymax < y)
						ymax = y;
					// Perimetro
					// Se pelo menos um dos quatro vizinhos nao pertence ao mesmo label, entao e um pixel de contorno
					if ((datasrc[pos - 1] != blobs[i].label) || (datasrc[pos + 1] != blobs[i].label) || (datasrc[pos - bytesperline] != blobs[i].label) || (datasrc[pos + bytesperline] != blobs[i].label))
					{
						blobs[i].perimeter++;
					}
				}
			}
		}

		// Bounding Box
		blobs[i].x = xmin;
		blobs[i].y = ymin;
		blobs[i].width = (xmax - xmin) + 1;
		blobs[i].height = (ymax - ymin) + 1;

		// Centro de Gravidade
		//blobs[i].xc = (xmax - xmin) / 2;
		//blobs[i].yc = (ymax - ymin) / 2;
		blobs[i].xc = sumx / MMAX(blobs[i].area, 1);
		blobs[i].yc = sumy / MMAX(blobs[i].area, 1);
	}
	return 1;
}

// Conversao de pixel para mm
float convertPXtoMM(int pixeis)
{
	float px = 0.1964;
	return ((float)pixeis * px);
}

// Elimina pequenos fragmentos da imagem
int vc_clean_image(IVC* src, IVC* dst, OVC blob)
{
	unsigned char* datasrc = (unsigned char*)src->data;
	unsigned char* datadst = (unsigned char*)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, i;
	long int pos_src, pos_blob;
	int xmin, ymin, xmax, ymax;
	long int sumx, sumy;

	//Verificacao de erros
	if (src == NULL)
	{
		printf("Error -> vc_binary_blob_info():\n\tImage is empy!\n");
		getchar();
		return 0;
	}

	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
	{
		printf("Error -> vc_binary_blob_info():\n\tImage Dimensions or data are missing!\n");
		getchar();
		return 0;
	}

	if (channels != 3 && channels != 1)
	{
		printf("Error -> vc_binary_blob_info():\n\tImages with incorrect format!\n");
		getchar();
		return 0;
	}


	for (y = blob.y; y <= blob.y + blob.height; y++)
	{
		for (x = blob.x; x <= blob.x + blob.width; x++)
		{
			pos_blob = y * src->bytesperline + x * src->channels;
			dst->data[pos_blob] = (unsigned char)0;
		}
	}
	return 1;
}

// Remove objetos cujos blobs coincidam mas as areas tenham uma grande % de diferenca entre elas
int vc_remove_apple(IVC* srcdst, OVC* info1, int n, OVC* info2, int m)
{
	unsigned char* data = (unsigned char*)srcdst->data;
	int bytesperline = srcdst->bytesperline;
	int channels = srcdst->channels;
	int width = srcdst->width;
	int height = srcdst->height;
	long int pos;

	// Verificacao de erros
	if ((srcdst->width <= 0) || (srcdst->height <= 0) || (srcdst->data == NULL)) return 0;
	if (srcdst->channels != 1) return 0;

	for (int i = 0; i < n; i++)
	{
		int status = 0;
		for (int j = 0; j < m; j++)
		{
			for (int x = info1[i].x; x < info1[i].x + info1[i].width; x++)
			{
				for (int y = info1[i].y; y < info1[i].y + info1[i].height; y++)
				{
					if (x >= info2[j].x && x <= info2[j].x + info2[j].width - 1 && y >= info2[j].y && y <= info2[j].y + info2[j].height - 1)
					{
						float perc = (float)(info1[i].area * 100) / info2[j].area;
						if (perc < 80 || perc > 110) vc_clean_image(srcdst, srcdst, info1[i]);
						status = 1;
						break;
					}
				}
				if (status == 1) break;
			}
			if (status == 1) break;
		}
	}
	return 1;
}

// Aplicacao das bounding boxes na imagem original
int vc_final_blob_info(IVC* src, IVC* dst, OVC* blobs, int* nblobs)
{
	unsigned char* datasrc = (unsigned char*)src->data;
	unsigned char* datadst = (unsigned char*)dst->data;
	int width = src->width;
	int height = src->height;
	int bytesperline = src->bytesperline;
	int channels = src->channels;
	int x, y, i;
	long int pos;
	int xmin, ymin, xmax, ymax;
	long int sumx, sumy;

	//Verificacao de erros
	if (src == NULL)
	{
		printf("Error -> vc_binary_blob_info():\n\tImage is empy!\n");
		getchar();
		return 0;
	}

	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL))
	{
		printf("Error -> vc_binary_blob_info():\n\tImage Dimensions or data are missing!\n");
		getchar();
		return 0;
	}

	if (channels != 3 && channels != 1)
	{
		printf("Error -> vc_binary_blob_info():\n\tImages with incorrect format!\n");
		getchar();
		return 0;
	}

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pos = i * src->bytesperline + j * src->channels;
			datadst[pos] = datasrc[pos];
			datadst[pos + 1] = datasrc[pos + 1];
			datadst[pos + 2] = datasrc[pos + 2];
		}
	}

	for (i = 0; i < *nblobs; i++)
	{
		//Ciclo que nos permite percorrer o blob de maneira a alterar o pixel da sua bounding box para branco
		for (y = blobs[i].y; y <= blobs[i].y + blobs[i].height; y++)
		{
			for (x = blobs[i].x; x <= blobs[i].x + blobs[i].width; x++)
			{
				pos = y * src->bytesperline + x * src->channels;

				if (y == blobs[i].y || y == blobs[i].y + blobs[i].height || x == blobs[i].x || x == blobs[i].x + blobs[i].width)
				{
					dst->data[pos] = (unsigned char)0;
					dst->data[pos + 1] = (unsigned char)0;
					dst->data[pos + 2] = (unsigned char)255;
				}
			}
		}

		//Ciclo que nos permite percorrer o blob de maneira a alterar os pixeis cruzados do seu centro de massa para preto assinalando-o no meio
		for (y = blobs[i].y; y <= blobs[i].y + blobs[i].height; y++)
		{
			for (x = blobs[i].x; x <= blobs[i].x + blobs[i].width; x++)
			{
				pos = y * src->bytesperline + x * src->channels;

				if ((y == blobs[i].yc && x == blobs[i].xc) || (y + 1 == blobs[i].yc && x == blobs[i].xc) || (y - 1 == blobs[i].yc && x == blobs[i].xc)
					|| (y == blobs[i].yc && x + 1 == blobs[i].xc) || (y == blobs[i].yc && x - 1 == blobs[i].xc) || (y + 2 == blobs[i].yc && x == blobs[i].xc)
					|| (y - 2 == blobs[i].yc && x == blobs[i].xc) || (y == blobs[i].yc && x + 2 == blobs[i].xc) || (y == blobs[i].yc && x - 2 == blobs[i].xc))
				{
					dst->data[pos] = (unsigned char)0;
					dst->data[pos + 1] = (unsigned char)0;
					dst->data[pos + 1] = (unsigned char)255;
				}
			}
		}
	}
	return 1;
}

// Aplica uma mascara na imagem original
int vc_final_img(IVC* src, IVC* dst, IVC* mask)
{
	int height_src = src->height, height_dst = dst->height, height_mask = mask->height;
	int width_src = src->width, width_dst = dst->width, width_mask = mask->width;
	if (height_src != height_dst || height_dst != height_mask)
		return 0;
	if (width_src != width_dst || width_dst != width_mask)
		return 0;
	if (src->channels != dst->channels || mask->channels != 1)
		return 0;

	int mult, pos;
	for (int i = 0; i < width_src * height_src; i++)
	{
		pos = i * src->channels;
		mult = mask->data[i] > 0;

		dst->data[pos] = src->data[pos] * mult;
		if (src->channels == 3) {
			dst->data[pos + 1] = src->data[pos + 1] * mult;
			dst->data[pos + 2] = src->data[pos + 2] * mult;
		}
	}

	return 1;
}

// Converter de RGB para Gray
int vc_rgb_to_gray(IVC* src, IVC* dst)
{
	unsigned char* datasrc = (unsigned char*)src->data;
	int bytesperline_src = src->width * src->channels;
	int channels_src = src->channels;
	unsigned char* datadst = (unsigned char*)dst->data;
	int bytesperline_dst = dst->width * dst->channels;
	int channels_dst = dst->channels;
	int width = src->width;
	int height = src->height;
	int x, y;
	long int pos_src, pos_dst;
	float rf, gf, bf;

	// VerificaCAo de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height)) return 0;
	if ((src->channels != 3) || (dst->channels != 1)) return 0;

	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			pos_src = y * bytesperline_src + x * channels_src;
			pos_dst = y * bytesperline_dst + x * channels_dst;

			rf = (float)datasrc[pos_src];
			gf = (float)datasrc[pos_src + 1];
			bf = (float)datasrc[pos_src + 2];

			datadst[pos_dst] = (unsigned char)((rf * 0.299) + (gf * 0.587) + (bf * 0.114));
		}
	}
	return 1;
}

// Erosao gray
int vc_gray_erode(IVC* src, IVC* dst, int kernel)
{
	unsigned char* data_src = (unsigned char*)src->data;
	int bytesperline_src = src->bytesperline;
	int channels_src = src->channels;
	int width = src->width;
	int height = src->height;
	int val = kernel / 2;
	int state = 0;

	unsigned char* data_dst = (unsigned char*)dst->data;
	int bytesperline_dst = dst->bytesperline;
	int channels_dst = dst->channels;
	long int pos_src, pos_dst, pos_pix;

	// VerificaCAo de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height)) return 0;
	if ((src->channels != 1) || (dst->channels != 1)) return 0;

	for (int i = 0; i < height; i++)
	{
		for (int j = 0; j < width; j++)
		{
			pos_src = i * bytesperline_src + j * channels_src;
			pos_dst = i * bytesperline_dst + j * channels_dst;

			state = data_src[pos_src];
			for (int x = i - val; x <= i + val; x++)
			{
				for (int y = j - val; y <= j + val; y++)
				{
					if (x >= 0 && x <= height && y >= 0 && y <= width)
					{
						pos_pix = x * bytesperline_src + y * channels_src;
						if (state > data_src[pos_pix])
							state = data_src[pos_pix];
					}
				}
			}
			data_dst[pos_dst] = state;
		}
	}
	return 1;
}

// Detecao de contornos pelos operadores Prewitt
int vc_gray_edge_prewitt(IVC* src, IVC* dst, float th)
{
	unsigned char* datasrc = (unsigned char*)src->data;
	unsigned char* datadst = (unsigned char*)dst->data;
	int width = src->width, height = src->height, bytesperline = src->bytesperline, channels = src->channels;
	int x, y, i, size, histthreshold, sumx, sumy;
	long int posX, posA, posB, posC, posD, posE, posF, posG, posH;
	float histmax, hist[256] = { 0.0f };

	// VerificaCAo de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;
	if ((src->width != dst->width) || (src->height != dst->height) || (src->channels != dst->channels)) return 0;
	if (channels != 1) return 0;

	size = width * height;

	for (y = 1; y < height - 1; y++)
	{
		for (x = 1; x < width - 1; x++)
		{
			// PosA PosB PosC
			// PosD PosX PosE
			// PosF PosG PosH

			posA = (y - 1) * bytesperline + (x - 1) * channels;
			posB = (y - 1) * bytesperline + x * channels;
			posC = (y - 1) * bytesperline + (x + 1) * channels;
			posD = y * bytesperline + (x - 1) * channels;
			posX = y * bytesperline + x * channels;
			posE = y * bytesperline + (x + 1) * channels;
			posF = (y + 1) * bytesperline + (x - 1) * channels;
			posG = (y + 1) * bytesperline + x * channels;
			posH = (y + 1) * bytesperline + (x + 1) * channels;

			// PosA*(-1) PosB*0 PosC*(1)
			// PosD*(-1) PosX*0 PosE*(1)
			// PosF*(-1) PosG*0 PosH*(1)

			sumx = datasrc[posA] * -1;
			sumx += datasrc[posD] * -1;
			sumx += datasrc[posF] * -1;

			sumx += datasrc[posC] * +1;
			sumx += datasrc[posE] * +1;
			sumx += datasrc[posH] * +1;
			sumx = sumx / 3; // 3 = 1 + 1 + 1

			// PosA*(-1) PosB*(-1) PosC*(-1)
			// PosD*0    PosX*0    PosE*0
			// PosF*(1)  PosG*(1)  PosH*(1)

			sumy = datasrc[posA] * -1;
			sumy += datasrc[posB] * -1;
			sumy += datasrc[posC] * -1;

			sumy += datasrc[posF] * +1;
			sumy += datasrc[posG] * +1;
			sumy += datasrc[posH] * +1;
			sumy = sumy / 3; // 3 = 1 + 1 + 1

			//datadst[posX] = (unsigned char)sqrt((double)(sumx*sumx + sumy*sumy));
			datadst[posX] = (unsigned char)(sqrt((double)(sumx * sumx + sumy * sumy)) / sqrt(2.0));
			// ExplicaCAo:
			// Queremos que no caso do pior cenário, em que sumx = sumy = 255, o resultado
			// da operaCAo se mantenha no intervalo de valores admitido, isto é, entre [0, 255].
			// Se se considerar que:
			// max = 255
			// Então,
			// sqrt(pow(max,2) + pow(max,2)) * k = max <=> sqrt(2*pow(max,2)) * k = max <=> k = max / (sqrt(2) * max) <=> 
			// k = 1 / sqrt(2)
		}
	}

	// Calcular o histograma com o valor das magnitudes
	for (i = 0; i < size; i++)
	{
		hist[datadst[i]]++;
	}

	// Definir o threshold.
	// O threshold é definido pelo nível de intensidade (das magnitudes)
	// quando se atinge uma determinada percentagem de pixeis, definida pelo utilizador.
	// Por exemplo, se o parâmetro 'th' tiver valor 0.8, significa the o threshold será o 
	// nível de magnitude, abaixo do qual estão pelo menos 80% dos pixeis.
	histmax = 0.0f;
	for (i = 0; i <= 255; i++)
	{
		histmax += hist[i];

		// th = Prewitt Threshold
		if (histmax >= (((float)size) * th)) break;
	}
	histthreshold = i == 0 ? 1 : i;

	// Aplicada o threshold
	for (i = 0; i < size; i++)
	{
		if (datadst[i] >= (unsigned char)histthreshold) datadst[i] = 255;
		else datadst[i] = 0;
	}

	return 1;
}

// Calcula a deformacao da imagem
int vc_calc_deformation(IVC* src, OVC* blob, int counter)
{
	unsigned char* data_src = (unsigned char*)src->data;
	int bytesperline_src = src->bytesperline;
	int channels_src = src->channels;
	int width = src->width;
	int height = src->height;
	long int pos_blob;

	// Verificacao de erros
	if ((src->width <= 0) || (src->height <= 0) || (src->data == NULL)) return 0;

	for (int i = 0; i < counter; i++)
	{
		int white = 0, black = 0, aux = 0;
		for (int y = blob[i].y; y <= blob[i].y + blob[i].height; y++)
		{
			for (int x = blob[i].x; x <= blob[i].x + blob[i].width; x++)
			{
				aux++;
				pos_blob = y * src->bytesperline + x * src->channels;
				if (data_src[pos_blob] == 255) white++;
				else black++;
			}
		}
		blob[i].deformation = (white * 100) / (float)blob[i].area;
	}
}