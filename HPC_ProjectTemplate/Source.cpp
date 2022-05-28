#include <iostream>
#include <math.h>
#include <stdlib.h>
#include<string.h>
#include<msclr\marshal_cppstd.h>
#include <ctime>// include this header 
#pragma once
#include <algorithm>
#include <cstdlib>
#include <mpi.h>
#include <cmath>
#using <mscorlib.dll>
#using <System.dll>
#using <System.Drawing.dll>
#using <System.Windows.Forms.dll>

using namespace std;
using namespace System;
using namespace msclr::interop;

int* inputImage(int* w, int* h, System::String^ imagePath, int expand_amount) //put the size of image in w & h
{
	int* input;


	int OriginalImageWidth, OriginalImageHeight;

	//*********************************************************Read Image and save it to local arrayss*************************	
	//Read Image and save it to local arrayss

	System::Drawing::Bitmap BM(imagePath);

	OriginalImageWidth = BM.Width;
	OriginalImageHeight = BM.Height;

	*w = BM.Width;
	*h = BM.Height;
	int* Red = new int[BM.Height * BM.Width];
	int* Green = new int[BM.Height * BM.Width];
	int* Blue = new int[BM.Height * BM.Width];
	input = new int[BM.Height * BM.Width];

	for (int i = 0; i < BM.Height; i++)
	{
		for (int j = 0; j < BM.Width; j++)
		{
			System::Drawing::Color c = BM.GetPixel(j, i);

			Red[i * BM.Width + j] = c.R;
			Blue[i * BM.Width + j] = c.B;
			Green[i * BM.Width + j] = c.G;

			input[i * BM.Width + j] = ((c.R + c.B + c.G) / 3); //gray scale value equals the average of RGB values

		}

	}
	return input;
}

void createImage(int* image, int width, int height, int index)
{
	System::Drawing::Bitmap MyNewImage(width, height);


	for (int i = 0; i < MyNewImage.Height; i++)
	{
		for (int j = 0; j < MyNewImage.Width; j++)
		{
			//i * OriginalImageWidth + j
			if (image[i * width + j] < 0)
			{
				image[i * width + j] = 0;
			}
			if (image[i * width + j] > 255)
			{
				image[i * width + j] = 255;
			}
			System::Drawing::Color c = System::Drawing::Color::FromArgb(image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j], image[i * MyNewImage.Width + j]);
			MyNewImage.SetPixel(j, i, c);
		}
	}
	MyNewImage.Save("..//Data//Output//outputRes" + index + ".png");
	cout << "-----------------\n"
		<< "Result Image " << index << " Saved"
		<< "\n-----------------\n" << endl;
}


int main()
{
	MPI_Init(NULL, NULL);
	int world_size;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	int rank;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

	MPI_Status recv_status;
	String^ imagePath;
	string img;
	img = "..//Data//Input//test1.jpg";
	imagePath = marshal_as<String^>(img);

	//Time Measure
	double startTime, endTime;
	//Select your Median Filter Window Size
	int windowSize = 25;
	//Preparing variables & Image Matrix to be used with the selected filter window
	int* imageData = new int[1];
	int ImageWidth = -1, ImageHeight = -1;
	int expand_amount = (sqrt(windowSize) - 1);
	int medianIndex = ceil(windowSize / 2);
	int* medianArr = new int[windowSize];
	int median_arr_Append = 0;
	int processed_image_arr_Append = 0;

	//Load image in the root core
	if (rank == 0) {
		imageData = inputImage(&ImageWidth, &ImageHeight, imagePath, expand_amount);

		cout << "\nStopwatch Started: \n" << endl;

		startTime = MPI_Wtime();
	}
	//Broadcast image size to other cores to be used in receiving assigned image part 
	if (world_size > 1) {
		MPI_Bcast(&ImageWidth, 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&ImageHeight, 1, MPI_INT, 0, MPI_COMM_WORLD);
	}

	//Preparing image chunks (equal to number of CPU cores) to be processed on each one in Parallel
	int number_rows_for_each = ceil((double)ImageHeight / world_size);
	int number_rows_for_Last_Core = ImageHeight - ((world_size - 1) * number_rows_for_each);
	int* raw_image_chunk = new int[(number_rows_for_each + expand_amount) * (ImageWidth)];
	int* processed_image_chunk = new int[number_rows_for_each * ImageWidth];
	int* Complete_Processed_imageData = new int[(ImageHeight + abs(number_rows_for_each - number_rows_for_Last_Core)) * ImageWidth];


	//Sending & receiving Unprocessed image chunks from the root to other Cores
	if (world_size > 1) {
		//Sending
		if (rank == 0) {
			for (int i = 0; i < ((number_rows_for_each + expand_amount / 2) * ImageWidth); i++) {
				raw_image_chunk[i] = imageData[i];
			}
			for (int i = 1; i < (world_size - 1); i++) {
				MPI_Send((imageData + i * (number_rows_for_each * ImageWidth) - (expand_amount / 2) * ImageWidth), ((number_rows_for_each + expand_amount) * ImageWidth), MPI_INT, i, i, MPI_COMM_WORLD);
			}
			MPI_Send((imageData + (world_size - 1) * ((number_rows_for_each)*ImageWidth) - (expand_amount / 2) * ImageWidth), ((number_rows_for_Last_Core + (expand_amount / 2)) * ImageWidth), MPI_INT, (world_size - 1), (world_size - 1), MPI_COMM_WORLD);
		}

		//Receiving
		if (rank != 0 && rank != (world_size - 1)) {
			MPI_Recv(raw_image_chunk, ((number_rows_for_each + expand_amount) * ImageWidth), MPI_INT, 0, rank, MPI_COMM_WORLD, &recv_status);
		}
		if (rank == (world_size - 1)) {
			MPI_Recv(raw_image_chunk, ((number_rows_for_Last_Core + (expand_amount / 2)) * ImageWidth), MPI_INT, 0, rank, MPI_COMM_WORLD, &recv_status);
		}
	}
	//In case of running the code in sequential mode: Prepare raw_image_chunk array
	else {
		for (int i = 0; i < ImageHeight * ImageWidth; i++) {
			raw_image_chunk[i] = imageData[i];
		}
	}

	//Processing First chunk of the image (handling Zero values at the top & sides)
	if (rank == 0) {

		for (int i = 0; i < number_rows_for_each; i++)
		{
			for (int j = 0; j < (ImageWidth); j++)
			{
				//initiat median filter matrix of current pixel of image
				for (int k = 0; k < sqrt(windowSize); k++) {
					for (int p = 0; p < sqrt(windowSize); p++) {
						if ((i + k) < expand_amount / 2
							|| (i + k) >= (expand_amount / 2 + ImageHeight)
							|| (j + p) < expand_amount / 2
							|| (j + p) >= (expand_amount / 2 + ImageWidth)) {

							medianArr[median_arr_Append] = 0;
							median_arr_Append++;
						}
						else {
							medianArr[median_arr_Append] 
								= raw_image_chunk[((i + k) - (expand_amount / 2)) * (ImageWidth)+((j + p) - (expand_amount / 2))];
							median_arr_Append++;
						}
					}
				}
				//Get the median value of current pixel 
					//then replace pixel value with the new median value
				sort(medianArr, medianArr + windowSize);
				processed_image_chunk[processed_image_arr_Append] = medianArr[medianIndex];
				processed_image_arr_Append++;
				median_arr_Append = 0;
			}
		}
	}

	//Processing Inner chunks of the image (handling Zero values at the sides only)
	if (rank != 0 && rank != (world_size - 1)) {
		for (int i = 0; i < number_rows_for_each; i++)
		{
			for (int j = 0; j < (ImageWidth); j++)
			{
				//initiat median filter matrix of current pixel of image
				for (int k = 0; k < sqrt(windowSize); k++) {
					for (int p = 0; p < sqrt(windowSize); p++) {
						if (
							(j + p) < expand_amount / 2
							|| (j + p) >= (expand_amount / 2 + ImageWidth)) {

							medianArr[median_arr_Append] = 0;
							median_arr_Append++;
						}
						else {
							medianArr[median_arr_Append] = raw_image_chunk[((i + k)) * (ImageWidth)+((j + p) - (expand_amount / 2))];
							median_arr_Append++;
						}
					}
				}
				//Get the median value of current pixel 
					//then replace pixel value with the new median value
				sort(medianArr, medianArr + windowSize);
				processed_image_chunk[processed_image_arr_Append] = medianArr[medianIndex];
				processed_image_arr_Append++;
				median_arr_Append = 0;
			}
		}
	}

	//Processing Last chunk of the image (handling Zero values at the bottom & sides)
	if (rank != 0 && rank == (world_size - 1)) {
		for (int i = 0; i < number_rows_for_Last_Core; i++)
		{
			for (int j = 0; j < (ImageWidth); j++)
			{
				//initiat median filter matrix of current pixel of image
				for (int k = 0; k < sqrt(windowSize); k++) {
					for (int p = 0; p < sqrt(windowSize); p++) {
						if ((i + k) >= (expand_amount / 2 + number_rows_for_Last_Core)
							|| (j + p) < expand_amount / 2
							|| (j + p) >= (expand_amount / 2 + ImageWidth)) {

							medianArr[median_arr_Append] = 0;
							median_arr_Append++;
						}
						else {

							medianArr[median_arr_Append] = raw_image_chunk[((i + k)) * (ImageWidth)+((j + p) - (expand_amount / 2))];
							median_arr_Append++;
						}
					}
				}
				//Get the median value of current pixel 
					//then replace pixel value with the new median value
				sort(medianArr, medianArr + windowSize);
				processed_image_chunk[processed_image_arr_Append] = medianArr[medianIndex];
				processed_image_arr_Append++;
				median_arr_Append = 0;

			}
		}
	}

	//Gathring image chunks from each CPU core at one of them (rank 0 as root)
	MPI_Gather(processed_image_chunk, number_rows_for_each * ImageWidth, MPI_INT, Complete_Processed_imageData, number_rows_for_each * ImageWidth, MPI_INT, 0, MPI_COMM_WORLD);

	if (rank == 0) {
		//Display Processing Time
		endTime = MPI_Wtime();
		if (world_size > 1)
			cout << "Parallel time: " << endTime - startTime << endl;
		else
			cout << "Seqential time: " << endTime - startTime << endl;

		//Save result image
		createImage(Complete_Processed_imageData, ImageWidth, ImageHeight, 0);
	}

	free(imageData);

	MPI_Finalize();
	return 0;

}