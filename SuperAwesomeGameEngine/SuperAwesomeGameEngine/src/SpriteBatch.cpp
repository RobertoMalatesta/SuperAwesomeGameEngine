// SpriteBatch.cpp

// GL Includes
#include <gl\glew.h>
// SAGE Includes
#include <SAGE\SpriteBatch.hpp>
// STL Includes
#include <algorithm>
#include <iostream>

namespace SAGE
{
	SpriteBatch::SpriteBatch()
	{
		mWithinDrawPair = false;
		mBufferIndex = 0;
	}

	SpriteBatch::~SpriteBatch()
	{
		Finalize();
	}

	bool SpriteBatch::Initialize()
	{
		GLsizei sizeUShort = sizeof(GLushort);
		GLsizei sizeFloat = sizeof(GLfloat);
		GLsizei sizeVPCNT = sizeof(VertexPositionColorNormalTexture);

		// Enable GLew.
		glewExperimental = GL_TRUE;
		GLenum res = glewInit();
		if (res != GLEW_OK)
		{
			SDL_Log("[Window::Initialize] GLEW failed to initialize: %s", glewGetErrorString(res));
			return false;
		}

		GLushort indexData[MaxIndexCount];
		for (int i = 0; i < MaxIndexCount / 6; i++)
		{
			indexData[i * 6 + 0] = i * 4 + 0;
			indexData[i * 6 + 1] = i * 4 + 1;
			indexData[i * 6 + 2] = i * 4 + 2;
			indexData[i * 6 + 3] = i * 4 + 0;
			indexData[i * 6 + 4] = i * 4 + 2;
			indexData[i * 6 + 5] = i * 4 + 3;
		}

		mIndexBufferObject = -1;
		glGenBuffers(1, &mIndexBufferObject);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBufferObject);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, MaxIndexCount * sizeUShort, indexData, GL_STATIC_DRAW);

		for (int i = 0; i < RingBufferCount; i++)
		{
			mVertexArrayObject[i] = -1;
			glGenVertexArrays(1, &mVertexArrayObject[i]);
			glBindVertexArray(mVertexArrayObject[i]);

			mVertexBufferObject[i] = -1;
			glGenBuffers(1, &mVertexBufferObject[i]);
			glBindBuffer(GL_ARRAY_BUFFER, mVertexBufferObject[i]);
			glBufferData(GL_ARRAY_BUFFER, MaxVertexCount * sizeVPCNT, nullptr, GL_DYNAMIC_DRAW);

			// Position
			glEnableVertexAttribArray(0);
			glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeVPCNT, (GLvoid*)(sizeFloat * 0));
			// Color
			glEnableVertexAttribArray(1);
			glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeVPCNT, (GLvoid*)(sizeFloat * 3));
			// Normal
			glEnableVertexAttribArray(2);
			glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeVPCNT, (GLvoid*)(sizeFloat * 7));
			// Texcoord
			glEnableVertexAttribArray(3);
			glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeVPCNT, (GLvoid*)(sizeFloat * 10));
		}

		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		return true;
	}

	bool SpriteBatch::Finalize()
	{
		for (int i = 0; i < RingBufferCount; i++)
		{
			glDeleteVertexArrays(1, &mVertexArrayObject[i]);
			glDeleteBuffers(1, &mVertexBufferObject[i]);
		}

		glDeleteBuffers(1, &mIndexBufferObject);

		mBatchItemList.clear();

		return true;
	}

	bool SpriteBatch::Begin(SortMode pSortMode, BlendMode pBlendMode, SamplerState pSamplerState, DepthStencilState pDepthStencilState, RasterizerState pRasterizerState)
	{
		if (mWithinDrawPair)
		{
			std::cout << "[SpriteBatch::Begin] Cannot nest draw pairs." << std::endl;
			return false;
		}

		mSortMode = pSortMode;
		mBlendMode = pBlendMode;
		mSamplerState = pSamplerState;
		mDepthStencilState = pDepthStencilState;
		mRasterizerState = pRasterizerState;

		mWithinDrawPair = true;

		return true;
	}

	bool SpriteBatch::Draw(const HGF::Texture& pTexture, const HGF::Vector2& pPosition, const HGF::Rectangle& pSource, const HGF::Color& pColor, const HGF::Vector2& pOrigin, float pRotation, const HGF::Vector2& pScale, OrientationEffect pOrientationEffect, float pDepth)
	{
		if (!mWithinDrawPair)
		{
			std::cout << "[SpriteBatch::Draw] Must start a draw pair first." << std::endl;
			return false;
		}

		int texWidth = pTexture.GetWidth();
		int texHeight = pTexture.GetHeight();

		HGF::Rectangle rect;
		if (pSource != HGF::Rectangle::Empty)
			rect = pSource;
		else
			rect = HGF::Rectangle(0, 0, texWidth, texHeight);

		HGF::Vector2 correction(1.0f / (float)texWidth, 1.0f / (float)texHeight);

		HGF::Vector2 size(rect.Width * pScale.X, rect.Height * pScale.Y);
		HGF::Vector2 origin(-pOrigin.X * pScale.X, -pOrigin.Y * pScale.Y);
		HGF::Vector2 texCoordTL(rect.X / (float)texWidth + correction.X, rect.Y / (float)texHeight + correction.X);
		HGF::Vector2 texCoordBR((rect.X + rect.Width) / (float)texWidth - correction.X, (rect.Y + rect.Height) / (float)texHeight - correction.Y);

		// Calculate cos/sin for rotation in radians.
		float cos = cosf(pRotation * (float)M_PI / 180.0f);
		float sin = sinf(pRotation * (float)M_PI / 180.0f);

		// Flip texture coordinates for orientation.
		if ((pOrientationEffect & OrientationEffect::FlipHorizontal) == OrientationEffect::FlipHorizontal)
			std::swap(texCoordTL.X, texCoordBR.X);
		if ((pOrientationEffect & OrientationEffect::FlipVertical) == OrientationEffect::FlipVertical)
			std::swap(texCoordTL.Y, texCoordBR.Y);

		SpriteBatchItem item;
		item.TextureID = pTexture.GetID();
		item.Depth = pDepth;

		item.VertexTL.Position.X = pPosition.X + origin.X * cos - origin.Y * sin;
		item.VertexTL.Position.Y = pPosition.Y + origin.X * sin + origin.Y * cos;
		item.VertexTL.Position.Z = pDepth;
		item.VertexTL.Color.R = pColor.GetRed();
		item.VertexTL.Color.G = pColor.GetGreen();
		item.VertexTL.Color.B = pColor.GetBlue();
		item.VertexTL.Color.A = pColor.GetAlpha();
		item.VertexTL.Normal.X = 0.0f;
		item.VertexTL.Normal.Y = 0.0f;
		item.VertexTL.Normal.Z = 1.0f;
		item.VertexTL.TexCoord.X = texCoordTL.X;
		item.VertexTL.TexCoord.Y = texCoordTL.Y;

		item.VertexTR.Position.X = pPosition.X + (origin.X + size.X) * cos - origin.Y * sin;
		item.VertexTR.Position.Y = pPosition.Y + (origin.X + size.X) * sin + origin.Y * cos;
		item.VertexTR.Position.Z = pDepth;
		item.VertexTR.Color.R = pColor.GetRed();
		item.VertexTR.Color.G = pColor.GetGreen();
		item.VertexTR.Color.B = pColor.GetBlue();
		item.VertexTR.Color.A = pColor.GetAlpha();
		item.VertexTR.Normal.X = 0.0f;
		item.VertexTR.Normal.Y = 0.0f;
		item.VertexTR.Normal.Z = 1.0f;
		item.VertexTR.TexCoord.X = texCoordBR.X;
		item.VertexTR.TexCoord.Y = texCoordTL.Y;

		item.VertexBL.Position.X = pPosition.X + origin.X * cos - (origin.Y + size.Y) * sin;
		item.VertexBL.Position.Y = pPosition.Y + origin.X * sin + (origin.Y + size.Y) * cos;
		item.VertexBL.Position.Z = pDepth;
		item.VertexBL.Color.R = pColor.GetRed();
		item.VertexBL.Color.G = pColor.GetGreen();
		item.VertexBL.Color.B = pColor.GetBlue();
		item.VertexBL.Color.A = pColor.GetAlpha();
		item.VertexBL.Normal.X = 0.0f;
		item.VertexBL.Normal.Y = 0.0f;
		item.VertexBL.Normal.Z = 1.0f;
		item.VertexBL.TexCoord.X = texCoordTL.X;
		item.VertexBL.TexCoord.Y = texCoordBR.Y;

		item.VertexBR.Position.X = pPosition.X + (origin.X + size.X) * cos - (origin.Y + size.Y) * sin;
		item.VertexBR.Position.Y = pPosition.Y + (origin.X + size.X) * sin + (origin.Y + size.Y) * cos;
		item.VertexBR.Position.Z = pDepth;
		item.VertexBR.Color.R = pColor.GetRed();
		item.VertexBR.Color.G = pColor.GetGreen();
		item.VertexBR.Color.B = pColor.GetBlue();
		item.VertexBR.Color.A = pColor.GetAlpha();
		item.VertexBR.Normal.X = 0.0f;
		item.VertexBR.Normal.Y = 0.0f;
		item.VertexBR.Normal.Z = 1.0f;
		item.VertexBR.TexCoord.X = texCoordBR.X;
		item.VertexBR.TexCoord.Y = texCoordBR.Y;

		mBatchItemList.push_back(item);

		return true;
	}

	bool SpriteBatch::End()
	{
		if (!mWithinDrawPair)
		{
			std::cout << "[SpriteBatch::End] Cannot end a pair without starting." << std::endl;
			return false;
		}

		switch (mSortMode)
		{
		case SortMode::Texture:
			std::sort(mBatchItemList.begin(), mBatchItemList.end(), [](const SpriteBatchItem& pItemA, const SpriteBatchItem& pItemB) { return pItemA.TextureID > pItemB.TextureID; });
			break;
		case SortMode::FrontToBack:
			std::sort(mBatchItemList.begin(), mBatchItemList.end(), [](const SpriteBatchItem& pItemA, const SpriteBatchItem& pItemB) { return pItemA.Depth > pItemB.Depth; });
			break;
		case SortMode::BackToFront:
			std::sort(mBatchItemList.begin(), mBatchItemList.end(), [](const SpriteBatchItem& pItemA, const SpriteBatchItem& pItemB) { return pItemA.Depth < pItemB.Depth; });
			break;
		}

		switch (mBlendMode)
		{
		case BlendMode::None:
			glDisable(GL_BLEND);
			break;
		case BlendMode::Premultiplied:
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case BlendMode::AlphaBlended:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case BlendMode::Additive:
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;
		}

		if (!mBatchItemList.empty())
			Render();

		mBatchItemList.clear();

		mWithinDrawPair = false;

		return true;
	}

	void SpriteBatch::Render()
	{
		int length = 0;
		int texID = -1;

		for (const auto& item : mBatchItemList)
		{
			if (item.TextureID != texID)
			{
				if (texID != -1)
				{
					Flush(texID, length);
				}
				length = 0;
				texID = item.TextureID;
			}

			if (length * 6 > MaxVertexCount)
			{
				Flush(texID, length);
				length = 0;
			}

			mVertexBuffer.push_back(item.VertexBL);
			mVertexBuffer.push_back(item.VertexBR);
			mVertexBuffer.push_back(item.VertexTR);
			mVertexBuffer.push_back(item.VertexTL);
			length++;
		}

		Flush(texID, length);

		mBufferIndex = (mBufferIndex + 1) % RingBufferCount;
	}

	void SpriteBatch::Flush(int pTextureID, int pLength)
	{
		// Ensure there's something to draw.
		if (pLength == 0)
			return;

		// Enable textures and bind the current ID.
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, pTextureID);

		// Bind the vertex array.
		glBindVertexArray(mVertexArrayObject[mBufferIndex]);

		// Bind the vertex buffer.
		glBindBuffer(GL_ARRAY_BUFFER, mVertexBufferObject[mBufferIndex]);

		// Option 1: Insert subset into buffer.
		glBufferSubData(GL_ARRAY_BUFFER, 0, pLength * 4 * sizeof(VertexPositionColorNormalTexture), mVertexBuffer.data());

		// Option 2: Write to buffer mapping.
		/*
		glBufferData(GL_ARRAY_BUFFER, mVertexBuffer.size() * sizeof(VertexPositionColorNormalTexture), nullptr, GL_DYNAMIC_DRAW);
		VertexPositionColorNormalTexture* map = reinterpret_cast<VertexPositionColorNormalTexture*>(
		glMapBufferRange(GL_ARRAY_BUFFER,
		0,
		pLength * sizeof(VertexPositionColorNormalTexture),
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)
		);
		std::copy(mVertexBuffer.begin(), mVertexBuffer.end(), map);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		*/

		// Bind the element buffer and draw.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mIndexBufferObject);
		glDrawElements(GL_TRIANGLES, pLength * 6, GL_UNSIGNED_SHORT, nullptr);

		// Clear bindings.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		// Disable textures.
		glDisable(GL_TEXTURE_2D);

		// Clear the current vertices.
		mVertexBuffer.clear();
	}
}