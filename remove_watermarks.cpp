#include "lab_m2/tema2/tema2.h"

#include <vector>
#include <iostream>
#include <chrono>
#include "pfd/portable-file-dialogs.h"
#include <omp.h>

using namespace std;


/*
 *  To find out more about `FrameStart`, `Update`, `FrameEnd`
 *  and the order in which they are called, see `world.cpp`.
 */


Tema2::Tema2()
{
	outputMode = 0;
	gpuProcessing = false;
	saveScreenToImage = false;
	window->SetSize(600, 600);
}


Tema2::~Tema2()
{
}


void Tema2::Init()
{
	// Load default texture fore imagine processing
	originalImage = TextureManager::LoadTexture(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::TEXTURES, "cube", "pos_x.png"), nullptr, "image", true, true);
	processedImage = TextureManager::LoadTexture(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::TEXTURES, "cube", "pos_x.png"), nullptr, "newImage", true, true);
	markerImage = TextureManager::LoadTexture(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::TEXTURES, "cube", "pos_x.png"), nullptr, "image", true, true);

	{
		Mesh* mesh = new Mesh("quad");
		mesh->LoadMesh(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::MODELS, "primitives"), "quad.obj");
		mesh->UseMaterials(false);
		meshes[mesh->GetMeshID()] = mesh;
	}

	std::string shaderPath = PATH_JOIN(window->props.selfDir, SOURCE_PATH::M2, "Tema2", "shaders");

	// Create a shader program for particle system
	{
		Shader* shader = new Shader("ImageProcessing");
		shader->AddShader(PATH_JOIN(shaderPath, "VertexShader.glsl"), GL_VERTEX_SHADER);
		shader->AddShader(PATH_JOIN(shaderPath, "FragmentShader.glsl"), GL_FRAGMENT_SHADER);

		shader->CreateAndLink();
		shaders[shader->GetName()] = shader;
	}
}


void Tema2::FrameStart()
{
}


void Tema2::Update(float deltaTimeSeconds)
{
	ClearScreen();

	auto shader = shaders["ImageProcessing"];
	shader->Use();

	if (saveScreenToImage)
	{
		window->SetSize(originalImage->GetWidth(), originalImage->GetHeight());
	}

	int flip_loc = shader->GetUniformLocation("flipVertical");
	glUniform1i(flip_loc, saveScreenToImage ? 0 : 1);

	int screenSize_loc = shader->GetUniformLocation("screenSize");
	glm::ivec2 resolution = window->GetResolution();
	glUniform2i(screenSize_loc, resolution.x, resolution.y);

	int outputMode_loc = shader->GetUniformLocation("outputMode");
	glUniform1i(outputMode_loc, outputMode);

	int locTexture = shader->GetUniformLocation("textureImage");
	glUniform1i(locTexture, 0);


	auto textureImage = (gpuProcessing == true) ? originalImage : processedImage;
	textureImage->BindToTextureUnit(GL_TEXTURE0);

	RenderMesh(meshes["quad"], shader, glm::mat4(1));

	if (saveScreenToImage)
	{
		saveScreenToImage = false;

		GLenum format = GL_RGB;
		if (originalImage->GetNrChannels() == 4)
		{
			format = GL_RGBA;
		}

		glReadPixels(0, 0, originalImage->GetWidth(), originalImage->GetHeight(), format, GL_UNSIGNED_BYTE, processedImage->GetImageData());
		processedImage->UploadNewData(processedImage->GetImageData());
		SaveImage("shader_processing_" + std::to_string(outputMode));

		float aspectRatio = static_cast<float>(originalImage->GetWidth()) / originalImage->GetHeight();
		window->SetSize(static_cast<int>(600 * aspectRatio), 600);
	}
}


void Tema2::FrameEnd()
{
	DrawCoordinateSystem();
}


void Tema2::OnFileSelected(const std::string& fileName, bool marker)
{
	if (fileName.size())
	{
		if (marker)
		{
			markerImage = TextureManager::LoadTexture(fileName, nullptr, "marker", true, true);
		}
		else
		{
			std::cout << fileName << endl;
			originalImage = TextureManager::LoadTexture(fileName, nullptr, "image", true, true);
			processedImage = TextureManager::LoadTexture(fileName, nullptr, "newImage", true, true);

			float aspectRatio = static_cast<float>(originalImage->GetWidth()) / originalImage->GetHeight();
			window->SetSize(static_cast<int>(600 * aspectRatio), 600);
		}
	}
}


void Tema2::GrayScale()
{
	unsigned int channels = originalImage->GetNrChannels();
	unsigned char* data = originalImage->GetImageData();
	unsigned char* newData = processedImage->GetImageData();

	if (channels < 3)
		return;

	glm::ivec2 imageSize = glm::ivec2(originalImage->GetWidth(), originalImage->GetHeight());

	for (int i = 0; i < imageSize.y; i++)
	{
		for (int j = 0; j < imageSize.x; j++)
		{
			int offset = channels * (i * imageSize.x + j);

			// Reset save image data
			char value = static_cast<char>(data[offset + 0] * 0.2f + data[offset + 1] * 0.71f + data[offset + 2] * 0.07);
			memset(&newData[offset], value, 3);
		}
	}

	processedImage->UploadNewData(newData);
}

void Tema2::RemoveMarkers() {
	// Record the start time for performance measurement
	auto start = std::chrono::high_resolution_clock::now();

	// Get image information
	unsigned int imageChannels = originalImage->GetNrChannels();
	unsigned char* data = originalImage->GetImageData();

	// Check if the image has at least 3 channels (RGB)
	if (imageChannels < 3) return;

	// Get marker information
	unsigned int markerChannels = markerImage->GetNrChannels();
	unsigned char* markerData = markerImage->GetImageData();

	// Check if the marker image has at least 3 channels (RGB)
	if (markerChannels < 3) return;

	// Get image and marker sizes
	glm::ivec2 imageSize = glm::ivec2(originalImage->GetWidth(), originalImage->GetHeight());
	glm::ivec2 markerSize = glm::ivec2(markerImage->GetWidth(), markerImage->GetHeight());

	// Sobel filter masks for edge detection
	int sobelX[3][3] = { {-1, 0, 1}, {-2, 0, 2}, {-1, 0, 1} };
	int sobelY[3][3] = { {-1, -2, -1}, {0, 0, 0}, {1, 2, 1} };

	// Create arrays to store Sobel-filtered images
	unsigned char* sobelImage = (unsigned char*)calloc(imageSize.x * imageSize.y, 1);
	unsigned char* sobelMarker = (unsigned char*)calloc(markerSize.x * markerSize.y, 1);

	// Apply Sobel filter on the original image
	for (int i = 1; i < imageSize.y - 1; i++) {
		for (int j = 1; j < imageSize.x - 1; j++) {
			int gx = 0;
			int gy = 0;

			// Apply Sobel filter masks
			for (int m = -1; m <= 1; m++) {
				for (int n = -1; n <= 1; n++) {
					int offset = imageChannels * ((i + m) * imageSize.x + (j + n));
					int gray = static_cast<unsigned char>(data[offset + 0] * 0.21f + data[offset + 1] * 0.71f + data[offset + 2] * 0.07);
					gx += sobelX[m + 1][n + 1] * gray;
					gy += sobelY[m + 1][n + 1] * gray;
				}
			}

			// Calculate magnitude and apply binarization
			int magnitude = static_cast<unsigned char>(sqrt(gx * gx + gy * gy));
			int threshold = 70;
			magnitude = magnitude > threshold ? 255 : 0;

			int offset = i * imageSize.x + j;
			sobelImage[offset] = magnitude;
		}
	}

	// Apply Sobel filter on the marker image
	for (int i = 1; i < markerSize.y - 1; i++) {
		for (int j = 1; j < markerSize.x - 1; j++) {
			int gx = 0;
			int gy = 0;

			// Apply Sobel filter masks
			for (int m = -1; m <= 1; m++) {
				for (int n = -1; n <= 1; n++) {
					int offset = markerChannels * ((i + m) * markerSize.x + (j + n));
					int gray = static_cast<unsigned char>(markerData[offset + 0] * 0.21f + markerData[offset + 1] * 0.71f + markerData[offset + 2] * 0.07);
					gx += sobelX[m + 1][n + 1] * gray;
					gy += sobelY[m + 1][n + 1] * gray;
				}
			}

			// Calculate magnitude and apply binarization
			int magnitude = static_cast<unsigned char>(sqrt(gx * gx + gy * gy));
			int threshold = 70;
			magnitude = magnitude > threshold ? 255 : 0;

			int offset = i * markerSize.x + j;
			sobelMarker[offset] = magnitude;
		}
	}

	// Iterate through the image to find and remove markers
	for (int i = 1; i < imageSize.y - markerSize.y; i++) {
		for (int j = 1; j < imageSize.x - markerSize.x; j++) {
			float total = 0, match = 0;

			// Iterate through the marker region
			for (int m = 0; m < markerSize.y; m++) {
				for (int n = 0; n < markerSize.x; n++) {
					int sobelMarkerOffset = m * markerSize.x + n;
					int sobelImageOffset = (i + m) * imageSize.x + (j + n);

					// Check if the pixel in the marker is white
					if (sobelMarker[sobelMarkerOffset] == 255) {
						total++;

						// Check if the corresponding pixel in the image is white
						if (sobelImage[sobelImageOffset] == 255) {
							match++;
						}
					}
				}
			}

			// Calculate matching ratio
			float ratio = total > 0 ? match / total : 0.0f;

			// Threshold for considering a match
			float matchThreshold = 0.8f;

			// Check if the matching ratio is above the threshold
			if (ratio >= matchThreshold) {
				// Iterate through the marker region again
				for (int m = 0; m < markerSize.y; m++) {
					for (int n = 0; n < markerSize.x; n++) {
						// Adjust the color values in the image
						for (int c = 0; c < imageChannels; c++) {
							int markerOffset = markerChannels * (m * markerSize.x + n) + c;
							int imageOffset = imageChannels * ((i + m) * imageSize.x + (j + n)) + c;

							int value = (int)data[imageOffset] - (int)markerData[markerOffset];
							if (value < 0)
								value += 255;

							data[imageOffset] = value;
						}
					}
				}

				// Print the position where the marker is found
				std::cout << "Marker found at " << "(" << i << ", " << j << ")" << '\n';

				// Jump to the next region after processing the marker
				j += markerSize.x - 1;
			}
		}
	}

	// Upload the modified data to the processed image
	processedImage->UploadNewData(data);

	// Record the finish time for performance measurement
	auto finish = std::chrono::high_resolution_clock::now();
	// Calculate and print the elapsed time
	std::chrono::duration<double> elapsed = finish - start;
	std::cout << "Elapsed time: " << elapsed.count() << "s\n";
}



void Tema2::SaveImage(const std::string& fileName)
{
	cout << "Saving image! ";
	processedImage->SaveToFile((fileName + ".png").c_str());
	cout << "[Done]" << endl;
}


void Tema2::OpenDialog()
{
	std::vector<std::string> filters =
	{
		"Image Files", "*.png *.jpg *.jpeg *.bmp",
		"All Files", "*"
	};

	auto selection = pfd::open_file("Select a file", ".", filters).result();
	if (!selection.empty())
	{
		std::cout << "User selected file " << selection[0] << "\n";
		OnFileSelected(selection[0], false);
	}

	selection = pfd::open_file("Select a file", ".", filters).result();
	if (!selection.empty())
	{
		std::cout << "User selected file " << selection[0] << "\n";
		OnFileSelected(selection[0], true);
	}
}


/*
 *  These are callback functions. To find more about callbacks and
 *  how they behave, see `input_controller.h`.
 */


void Tema2::OnInputUpdate(float deltaTime, int mods)
{
	// Treat continuous update based on input
}


void Tema2::OnKeyPress(int key, int mods)
{
	// Add key press event
	if (key == GLFW_KEY_F || key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE)
	{
		OpenDialog();
	}

	if (key == GLFW_KEY_E)
	{
		gpuProcessing = !gpuProcessing;
		if (gpuProcessing == false)
		{
			outputMode = 0;
		}
		cout << "Processing on GPU: " << (gpuProcessing ? "true" : "false") << endl;
	}

	if (key - GLFW_KEY_0 >= 0 && key < GLFW_KEY_3)
	{
		outputMode = key - GLFW_KEY_0;

		if (gpuProcessing == false)
		{
			outputMode = 0;
			RemoveMarkers();
		}
	}

	if (key == GLFW_KEY_S && mods & GLFW_MOD_CONTROL)
	{
		if (!gpuProcessing)
		{
			SaveImage("processCPU_" + std::to_string(outputMode));
		}
		else {
			saveScreenToImage = true;
		}
	}
}


void Tema2::OnKeyRelease(int key, int mods)
{
	// Add key release event
}


void Tema2::OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY)
{
	// Add mouse move event
}


void Tema2::OnMouseBtnPress(int mouseX, int mouseY, int button, int mods)
{
	// Add mouse button press event
}


void Tema2::OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods)
{
	// Add mouse button release event
}


void Tema2::OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY)
{
	// Treat mouse scroll event
}


void Tema2::OnWindowResize(int width, int height)
{
	// Treat window resize event
}
