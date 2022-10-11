#include <iostream>
#include <string>
#include <opencv2\opencv.hpp>
#include <opencv2\core.hpp>
#include <opencv2\highgui.hpp>
#include <opencv2\videoio.hpp>

extern "C" {
#include "vc.h"

}

int main(void) {
	// Video
	char videofile[20] = "video.avi";
	cv::VideoCapture capture;
	struct
	{
		int width, height;
		int ntotalframes;
		int fps;
		int nframe;
	} video;
	// Outros
	std::string str;
	int key = 0;

	/* Leitura de video de um ficheiro */
	/* NOTA IMPORTANTE:
	O ficheiro video.avi devera estar localizado no mesmo directorio que o ficheiro de codigo fonte.
	*/
	capture.open(videofile);

	/* Em alternativa, abrir captura de video pela Webcam #0 */
	//capture.open(0, cv::CAP_DSHOW); // Pode-se utilizar apenas capture.open(0);

	/* Verifica se foi possivel abrir o ficheiro de video */
	if (!capture.isOpened())
	{
		std::cerr << "Erro ao abrir o ficheiro de video!\n";
		return 1;
	}

	/* Número total de frames no video */
	video.ntotalframes = (int)capture.get(cv::CAP_PROP_FRAME_COUNT);
	/* Frame rate do video */
	video.fps = (int)capture.get(cv::CAP_PROP_FPS);
	/* Resolucao do video */
	video.width = (int)capture.get(cv::CAP_PROP_FRAME_WIDTH);
	video.height = (int)capture.get(cv::CAP_PROP_FRAME_HEIGHT);

	/* Cria uma janela para exibir o video */
	cv::namedWindow("VC - VIDEO", cv::WINDOW_AUTOSIZE);

	cv::Mat frame, framergb;
	int nFrames = 0;
	while (key != 'q') {
		/* Leitura de uma frame do video */
		capture.read(frame);
		if (nFrames++ % 10 != 0) continue;
		int n, m, counter1, counter2;

		/* Verifica se conseguiu ler a frame */
		if (frame.empty()) break;
		cv::cvtColor(frame, framergb, cv::COLOR_BGR2RGB);
		/* Numero da frame a processar */
		video.nframe = (int)capture.get(cv::CAP_PROP_POS_FRAMES);

		// Cria uma nova imagem IVC
		IVC* src = vc_image_new(video.width, video.height, 3, 255);				// Armazena a imagem de cada frame
		IVC* hsv = vc_image_new(video.width, video.height, 3, 255);				// Armazena o resultado da transformacao de rgb para hsv
		IVC* hsv_s1 = vc_image_new(video.width, video.height, 1, 255);			// Armazena a segmentação em HSV das laranjas
		IVC* hsv_s2 = vc_image_new(video.width, video.height, 1, 255);			// Armazena a segmentação em HSV da mesa
		IVC* b1 = vc_image_new(video.width, video.height, 1, 255);				// Armazena o fecho binario da segmentacao das laranjas
		IVC* b2 = vc_image_new(video.width, video.height, 1, 255);				// Armazena o negativo binario da segmentacao da mesa
		IVC* blobs1 = vc_image_new(video.width, video.height, 1, 255);			// Armazena as etiquetas de b1
		IVC* blobs2 = vc_image_new(video.width, video.height, 1, 255);			// Armazena as etiquetas de b2
		IVC* blobs3 = vc_image_new(video.width, video.height, 1, 255);			// Armazena as etiquetas finais
		IVC* blobsa1 = vc_image_new(video.width, video.height, 1, 255);			// Escreve a bounding box e o centro dos blobs de b1
		IVC* blobsa2 = vc_image_new(video.width, video.height, 1, 255);			// Escreve a bounding box e o centro dos blobs de b2
		IVC* blobsa3 = vc_image_new(video.width, video.height, 1, 255);			// Escreve a bounding box e o centro dos blobs finais
		IVC* partial_dst = vc_image_new(video.width, video.height, 3, 255);		// Armazena uma imagem parcial com as laranjas a cores e o restante a preto
		IVC* gray1 = vc_image_new(video.width, video.height, 1, 255);			// Armazena a imagem anterior convertida para tons de cinzento
		IVC* gray2 = vc_image_new(video.width, video.height, 1, 255);			// Armazena a imagem anterior com a aplicacao de uma erosao
		IVC* prewitt = vc_image_new(video.width, video.height, 1, 255);			// Armazena a imagem anterior com os contornos prewitt
		IVC* dst = vc_image_new(video.width, video.height, 3, 255);				// Armazena a imagem final (imagem source com a bounding box nas laranjas)

		// Copia dados de imagem da estrutura cv::Mat para uma estrutura IVC
		memcpy(src->data, framergb.data, video.width * video.height * 3);

		// Execução de funcoes da nossa biblioteca
		vc_rgb_to_hsv(src, hsv);												// Converte a imagem original para HSV

		vc_hsv_segmentation(hsv, hsv_s1, 15, 30, 50, 100, 25, 90);				// Segmenta as laranjas da imagem
		vc_hsv_segmentation(hsv, hsv_s2, 0, 360, 0, 20, 30, 100);				// Segmenta a mesa da laranja

		vc_binary_close(hsv_s1, b1, 27, 10);									// Efetua o fecho binario de b1
		vc_binary_negative(hsv_s2, b2);											// Efetua o negativo binario de b2

		OVC* info1 = vc_binary_blob_labelling(b1, blobs1, &n);					// Efetua a etiquetagem e armazena os blobs de b1
		OVC* info2 = vc_binary_blob_labelling(b2, blobs2, &m);					// Efetua a etiquetagem e armazena os blobs de b2

		vc_binary_blob_info(blobs1, blobsa1, info1, &n);						// Calcula as informacoes de cada blob (area, perimetros, etc) de b1
		vc_binary_blob_info(blobs2, blobsa2, info2, &m);						// Calcula as informacoes de cada blob (area, perimetros, etc) de b2

		// Neste ciclo, todas as partes da imagem menores que a medida minima exigida das laranjas sao removidas em b1
		counter1 = n;
		for (int i = 0; i < n; i++)
		{
			if (convertPXtoMM(info1[i].width) < 53)
			{
				vc_clean_image(b1, b1, info1[i]);
				counter1--;
			}
		}

		info1 = vc_binary_blob_labelling(b1, blobs1, &counter1);				// Efetua nova etiquetagem de b1 após a eliminação de pequenos fragmentos
		vc_binary_blob_info(blobs1, blobsa1, info1, &counter1);					// Calcula as informacoes de cada blob (area, perimetros, etc) de b1
		vc_remove_apple(b1, info1, counter1, info2, m);							// Compara os blobs de b1 com os blobs de b2, se a area for menor que 80% ou maior que 110% os blobs são removidos (remove a maça deformada)

		// Neste ciclo, o blob é removido se estiver nas bordas inferior ou superior da imagem
		for (int i = 0; i < counter1; i++)
		{
			if (info1[i].y <= 5 || info1[i].y + info1[i].height + 5 >= src->height)
			{
				vc_clean_image(b1, b1, info1[i]);
			}
		}

		OVC* infofinal = vc_binary_blob_labelling(b1, blobs3, &counter1);		// Efetua a etiquetagem da imagem final, após passar por todos os processos anteriores
		vc_binary_blob_info(blobs3, blobsa3, infofinal, &counter1);				// Calcula as informações dos novos blobs
		vc_final_blob_info(src, dst, infofinal, &counter1);						// Aplica as boundingbox na imagem original

		vc_final_img(src, partial_dst, b1);										// Aplica um filtro onde só se vê as laranjas e tudo o resto fica a preto
		vc_rgb_to_gray(dst, gray1);												// Converte a imagem anterior para tons de cinza
		vc_gray_erode(gray1, gray2, 10);										// Aplica uma erosão à imagem anterior
		vc_gray_edge_prewitt(gray2, prewitt, 0.992);							// Obtém os contornos prewitt
		vc_calc_deformation(prewitt, infofinal, counter1);						// Efetua o cálculo da deformação e armazena no campo criado para o efeito na estrutura OVC

		// Copia dados de imagem da estrutura IVC para uma estrutura cv::Mat
		memcpy(frame.data, dst->data, video.width * video.height * 3);
		cv::cvtColor(frame, frame, cv::COLOR_RGB2BGR);

		// Escrever os dados obtidos em cada frame
		if (infofinal != NULL)
		{
			for (int i = 0; i < counter1; i++)
			{			
				float diam = convertPXtoMM(infofinal[i].width);
				str = std::string("AREA: ").append(std::to_string(infofinal[i].area));
				cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc - 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
				cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc - 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				str = std::string("PERIMETRO: ").append(std::to_string(infofinal[i].area));
				cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
				cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				str = std::string("DIAMETRO: ").append(std::to_string(diam).append("mm"));
				cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
				cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					
				#pragma region Verificacao de Calibre
					if (diam > 53 && diam < 60)
					{
						str = std::string("CALIBRE: ").append(std::to_string(13));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 56 && diam < 63)
					{
						str = std::string("CALIBRE: ").append(std::to_string(12));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 58 && diam < 66)
					{
						str = std::string("CALIBRE: ").append(std::to_string(11));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 60 && diam < 68)
					{
						str = std::string("CALIBRE: ").append(std::to_string(10));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 62 && diam < 70)
					{
						str = std::string("CALIBRE: ").append(std::to_string(9));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 64 && diam < 73)
					{
						str = std::string("CALIBRE: ").append(std::to_string(8));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 67 && diam < 76)
					{
						str = std::string("CALIBRE: ").append(std::to_string(7));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 70 && diam < 80)
					{
						str = std::string("CALIBRE: ").append(std::to_string(6));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 73 && diam < 84)
					{
						str = std::string("CALIBRE: ").append(std::to_string(5));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 77 && diam < 88)
					{
						str = std::string("CALIBRE: ").append(std::to_string(4));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 81 && diam < 92)
					{
						str = std::string("CALIBRE: ").append(std::to_string(3));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 84 && diam < 96)
					{
						str = std::string("CALIBRE: ").append(std::to_string(2));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam > 87 && diam < 100)
					{
						str = std::string("CALIBRE: ").append(std::to_string(1));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else if (diam >= 100)
					{
						str = std::string("CALIBRE: ").append(std::to_string(0));
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					else
					{
						str = std::string("CALIBRE FORA DO REGULAMENTO");
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
						cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					}
					#pragma endregion
				
				#pragma region Verificacao de deformacao
				str = std::string("DEFORMACAO: ").append(std::to_string(infofinal[i].deformation).append("%"));
				cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
				cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				
				if (infofinal[i].deformation > 0 && infofinal[i].deformation < 0.2)
				{
					str = std::string("CATEGORIA: EXTRA");
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					str = std::string("APROVADO: SIM");
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				}
				if (infofinal[i].deformation > 0.2 && infofinal[i].deformation < 0.7)
				{
					str = std::string("CATEGORIA: I");
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					str = std::string("APROVADO: SIM");
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				}
				if (infofinal[i].deformation > 0.7 && infofinal[i].deformation < 1)
				{
					str = std::string("CATEGORIA: II");
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					str = std::string("APROVADO: SIM");
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				}
				if (infofinal[i].deformation > 1)
				{
					str = std::string("CATEGORIA: III");
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
					str = std::string("APROVADO: NAO");
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
					cv::putText(frame, str, cv::Point(infofinal[i].xc - 150, infofinal[i].yc + 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
				}
				#pragma endregion
			}
			free(infofinal);	// Liberta a memória de armazenamento dos blobs finais
			free(info1);		// Liberta a memória de armazenamento dos blobs da info1
			free(info2);		// Liberta a memória de armazenamento dos blobs da info2
		}

		// Liberta a memoria das imagens IVC anteriormente criadas
		vc_image_free(src);
		vc_image_free(hsv);
		vc_image_free(hsv_s1);
		vc_image_free(hsv_s1);
		vc_image_free(b1);
		vc_image_free(b2);
		vc_image_free(blobs1);
		vc_image_free(blobs2);
		vc_image_free(blobs3);
		vc_image_free(blobsa1);
		vc_image_free(blobsa2);
		vc_image_free(blobsa3);
		vc_image_free(partial_dst);
		vc_image_free(gray1);
		vc_image_free(gray2);
		vc_image_free(prewitt);
		vc_image_free(dst);

		/* Exemplo de insercao texto na frame */
		str = std::string("RESOLUCAO: ").append(std::to_string(video.width)).append("x").append(std::to_string(video.height));
		cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 25), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
		str = std::string("TOTAL DE FRAMES: ").append(std::to_string(video.ntotalframes));
		cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 50), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
		str = std::string("FRAME RATE: ").append(std::to_string(video.fps));
		cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 75), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
		str = std::string("N. DE FRAMES: ").append(std::to_string(video.nframe));
		cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 100), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);
		str = std::string("LARANJAS: ").append(std::to_string(counter1));
		cv::putText(frame, str, cv::Point(20, 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(0, 0, 0), 2);
		cv::putText(frame, str, cv::Point(20, 125), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 1);

		/* Exibe a frame */
		cv::imshow("VC - VIDEO", frame);

		/* Sai da aplicacao, se o utilizador premir a tecla 'q' */
		key = cv::waitKey(1);
	}

	/* Fecha a janela */
	cv::destroyWindow("VC - VIDEO");

	/* Fecha o ficheiro de video */
	capture.release();

	return 0;
}