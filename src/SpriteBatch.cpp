// SpriteBatch.cpp

// OpenGL Includes
#include <GL/glew.h>
// SAGE Includes
#include <SAGE/Math.hpp>
#include <SAGE/SpriteBatch.hpp>
// STL Includes
#include <algorithm>

namespace SAGE
{
	SpriteBatch::SpriteBatch()
	{
		m_WithinDrawPair = false;
		m_ItemCount = 0;
		m_FlushCount = 0;
	}

	SpriteBatch::~SpriteBatch()
	{
		Finalize();
	}

	int SpriteBatch::GetDrawCallCount() const
	{
		return m_FlushCount;
	}

	bool SpriteBatch::Initialize()
	{
		// Collect type sizes.
		GLsizei sizeUShort = sizeof(GLushort);
		GLsizei sizeFloat = sizeof(GLfloat);
		GLsizei sizeVPCT = sizeof(VertexPositionColorTexture);

		// Enable GLew.
		glewExperimental = GL_TRUE;
		GLenum res = glewInit();
		if (res != GLEW_OK)
		{
			SDL_Log("[SpriteBatch::Initialize] GLEW failed to initialize: %s", glewGetErrorString(res));
			return false;
		}

		// Generate data for index buffer.
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

		// Create the index buffer object.
		m_IndexBufferObject = -1;
		glGenBuffers(1, &m_IndexBufferObject);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBufferObject);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, MaxIndexCount * sizeUShort, indexData, GL_STATIC_DRAW);

		// Create the vertex array object.
		m_VertexArrayObject = -1;
		glGenVertexArrays(1, &m_VertexArrayObject);
		glBindVertexArray(m_VertexArrayObject);

		// Create the vertex buffer object.
		m_VertexBufferObject = -1;
		glGenBuffers(1, &m_VertexBufferObject);
		glBindBuffer(GL_ARRAY_BUFFER, m_VertexBufferObject);
		glBufferData(GL_ARRAY_BUFFER, MaxVertexCount * sizeVPCT, nullptr, GL_DYNAMIC_DRAW);

		#define BUFFER_OFFSET(i) ((char*)0 + (i))

		// Position attribute
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeVPCT, BUFFER_OFFSET(sizeFloat * 0));
		// Color attribute
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeVPCT, BUFFER_OFFSET(sizeFloat * 2));
		// Texcoord attribute
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeVPCT, BUFFER_OFFSET(sizeFloat * 6));

		#undef BUFFER_OFFSET

		// Clear bindings.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		// Create the blank texture.
		std::vector<Color> colors = { Color::White };
		if (!m_BlankTexture.FromPixelColors(1, 1, colors))
		{
			SDL_Log("[SpriteBatch::Initialize] Failed to create 1x1 pixel blank texture.");
			return false;
		}

		return true;
	}

	bool SpriteBatch::Finalize()
	{
		// Destroy objects.
		glDeleteVertexArrays(1, &m_VertexArrayObject);
		glDeleteBuffers(1, &m_VertexBufferObject);
		glDeleteBuffers(1, &m_IndexBufferObject);

		return true;
	}

	bool SpriteBatch::Begin(Effect& p_Effect, const Camera2D& p_Camera, SortMode p_SortMode, BlendMode p_BlendMode, RasterizerState p_RasterizerState)
	{
		if (m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::Begin] Cannot nest draw pairs.");
			return false;
		}

		// Initialize drawing values.
		m_WithinDrawPair = true;
		m_ItemCount = 0;
		m_FlushCount = 0;

		// Store parameters.
		m_SortMode = p_SortMode;
		m_BlendMode = p_BlendMode;
		m_RasterizerState = p_RasterizerState;

		// Set matrices from camera.
		p_Effect.SetProjection(p_Camera.GetProjectionMatrix());
		p_Effect.SetModelView(p_Camera.GetModelViewMatrix());
		p_Effect.Use();

		return true;
	}

	bool SpriteBatch::DrawLine(const Vector2& p_PositionA, const Vector2& p_PositionB, const Color& p_Color, float p_Thickness, float p_Depth)
	{
		return DrawLine(p_PositionA, p_Color, p_PositionB, p_Color, p_Thickness, p_Depth);
	}

	bool SpriteBatch::DrawLine(const Vector2& p_PositionA, const Color& p_ColorA, const Vector2& p_PositionB, const Color& p_ColorB, float p_Thickness, float p_Depth)
	{
		if (!m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::DrawLine] Must start a draw pair first.");
			return false;
		}

		// Create the identity vector perpendicular to the line.
		Vector2 perpendicular = Vector2(p_PositionA.Y - p_PositionB.Y, p_PositionB.X - p_PositionA.X);
		perpendicular.Normalize();

		SpriteBatchItem& item = GetNextItem(m_BlankTexture, p_Depth);

		// Top left vertex.
		SetVertexPosition(item.VertexTL, p_PositionA.X + perpendicular.X * p_Thickness / 2.0f,
										 p_PositionA.Y + perpendicular.Y * p_Thickness / 2.0f);
		SetVertexColor(item.VertexTL, p_ColorA);
		SetVertexTexCoords(item.VertexTL, 0.0f, 1.0f);

		// Top right vertex.
		SetVertexPosition(item.VertexTR, p_PositionA.X - perpendicular.X * p_Thickness / 2.0f,
										 p_PositionA.Y - perpendicular.Y * p_Thickness / 2.0f);
		SetVertexColor(item.VertexTR, p_ColorA);
		SetVertexTexCoords(item.VertexTR, 1.0f, 1.0f);

		// Bottom left vertex.
		SetVertexPosition(item.VertexBL, p_PositionB.X + perpendicular.X * p_Thickness / 2.0f,
										 p_PositionB.Y + perpendicular.Y * p_Thickness / 2.0f);
		SetVertexColor(item.VertexBL, p_ColorB);
		SetVertexTexCoords(item.VertexBL, 0.0f, 0.0f);

		// Bottom right vertex.
		SetVertexPosition(item.VertexBR, p_PositionB.X - perpendicular.X * p_Thickness / 2.0f,
										 p_PositionB.Y - perpendicular.Y * p_Thickness / 2.0f);
		SetVertexColor(item.VertexBR, p_ColorB);
		SetVertexTexCoords(item.VertexBR, 1.0f, 0.0f);

		return true;
	}

	bool SpriteBatch::DrawLines(const std::vector<Vector2>& p_Positions, const Color& p_Color, float p_Thickness, float p_Depth)
	{
		std::vector<Color> colors = { p_Color };

		return DrawLines(p_Positions, colors, p_Thickness, p_Depth);
	}

	bool SpriteBatch::DrawLines(const std::vector<Vector2>& p_Positions, const std::vector<Color>& p_Colors, float p_Thickness, float p_Depth)
	{
		if (!m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::DrawLine] Must start a draw pair first.");
			return false;
		}

		int positionCount = static_cast<int>(p_Positions.size());

		if (positionCount < 2)
		{
			SDL_Log("[SpriteBatch::DrawLine] Position vector must contain at least 2 values.");
			return false;
		}

		int colorCount = static_cast<int>(p_Colors.size());

		for (int i = 0; i < positionCount - 1; ++i)
		{
			if (!DrawLine(p_Positions[i], p_Colors[i % colorCount], p_Positions[i + 1], p_Colors[(i + 1) % colorCount], p_Thickness, p_Depth))
			{
				return false;
			}
		}

		return true;
	}

	bool SpriteBatch::DrawLineLoop(const std::vector<Vector2>& p_Positions, const Color& p_Color, float p_Thickness, float p_Depth)
	{
		std::vector<Color> colors = { p_Color };

		return DrawLineLoop(p_Positions, colors, p_Thickness, p_Depth);
	}

	bool SpriteBatch::DrawLineLoop(const std::vector<Vector2>& p_Positions, const std::vector<Color>& p_Colors, float p_Thickness, float p_Depth)
	{
		if (!m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::DrawLineLoop] Must start a draw pair first.");
			return false;
		}

		int positionCount = static_cast<int>(p_Positions.size());

		if (positionCount < 3)
		{
			SDL_Log("[SpriteBatch::DrawLineLoop] Position vector must contain at least 3 values.");
			return false;
		}

		int colorCount = static_cast<int>(p_Colors.size());

		for (int i = 0; i < positionCount; ++i)
		{
			if (!DrawLine(p_Positions[i % positionCount], p_Colors[i % colorCount],
						  p_Positions[(i + 1) % positionCount], p_Colors[(i + 1) % colorCount],
						  p_Thickness, p_Depth))
			{
				return false;
			}
		}

		return true;
	}

	bool SpriteBatch::DrawCircle(const Vector2& p_Position, const Color& p_Color, float p_Radius, float p_Thickness, float p_Depth)
	{
		std::vector<Color> colors = { p_Color };

		return DrawCircle(p_Position, colors, p_Radius, p_Thickness, p_Depth);
	}

	bool SpriteBatch::DrawCircle(const Vector2& p_Position, const std::vector<Color>& p_Colors, float p_Radius, float p_Thickness, float p_Depth)
	{
		if (!m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::DrawCircle] Must start a draw pair first.");
			return false;
		}

		std::vector<Vector2> positions;
		for (float angle = 0.0f; angle < Math::TwoPi; angle += Math::Pi / 16.0f)
		{
			float cosAngle = cosf(angle);
			float sinAngle = sinf(angle);

			positions.push_back(Vector2(p_Position.X + p_Radius * cosAngle,
										p_Position.Y + p_Radius * sinAngle));
		}

		return DrawLineLoop(positions, p_Colors, p_Thickness, p_Depth);
	}

	bool SpriteBatch::DrawSolidRectangle(float p_X, float p_Y, float p_Width, float p_Height, const Color& p_Color, float p_Depth)
	{
		if (!m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::DrawSolidRectangle] Must start a draw pair first.");
			return false;
		}

		SpriteBatchItem& item = GetNextItem(m_BlankTexture, p_Depth);

		// Top left vertex.
		SetVertexPosition(item.VertexTL, p_X, p_Y);
		SetVertexColor(item.VertexTL, p_Color);
		SetVertexTexCoords(item.VertexTL, 0.0f, 1.0f);

		// Top right vertex.
		SetVertexPosition(item.VertexTR, p_X + p_Width, p_Y);
		SetVertexColor(item.VertexTR, p_Color);
		SetVertexTexCoords(item.VertexTR, 1.0f, 1.0f);

		// Bottom left vertex.
		SetVertexPosition(item.VertexBL, p_X, p_Y + p_Height);
		SetVertexColor(item.VertexBL, p_Color);
		SetVertexTexCoords(item.VertexBL, 0.0f, 0.0f);

		// Bottom right vertex.
		SetVertexPosition(item.VertexBR, p_X + p_Width, p_Y + p_Height);
		SetVertexColor(item.VertexBR, p_Color);
		SetVertexTexCoords(item.VertexBR, 1.0f, 0.0f);

		return true;
	}

	bool SpriteBatch::DrawSolidRectangle(const Rectangle& p_Rectangle, const Color& p_Color, float p_Depth)
	{
		return DrawSolidRectangle(static_cast<float>(p_Rectangle.X),
								  static_cast<float>(p_Rectangle.Y),
								  static_cast<float>(p_Rectangle.Width),
								  static_cast<float>(p_Rectangle.Height),
								  p_Color, p_Depth);
	}

	bool SpriteBatch::DrawSolidRectangle(const Vector2& p_Position, const Vector2& p_Dimensions, const Color& p_Color, float p_Depth)
	{
		return DrawSolidRectangle(p_Position.X, p_Position.Y, p_Dimensions.X, p_Dimensions.Y, p_Color, p_Depth);
	}

	bool SpriteBatch::DrawSprite(const Texture& p_Texture, const Vector2& p_Position, const Vector2& p_Dimensions, const Rectangle& p_SourceRectangle, const Color& p_Color, const Vector2& p_Origin, float p_Rotation, const Vector2& p_Scale, Orientation p_Orientation, float p_Depth)
	{
		// TODO: Factor in p_Dimensions.
		return DrawSprite(p_Texture, p_Position, p_SourceRectangle, p_Color, p_Origin, p_Rotation, p_Scale, p_Orientation, p_Depth);
	}

	bool SpriteBatch::DrawSprite(const Texture& p_Texture, const Vector2& p_Position, const Rectangle& p_DestinationRectangle, const Rectangle& p_SourceRectangle, const Color& p_Color, const Vector2& p_Origin, float p_Rotation, const Vector2& p_Scale, Orientation p_Orientation, float p_Depth)
	{
		// TODO: Factor in p_DestinationRectangle.
		return DrawSprite(p_Texture, p_Position, p_SourceRectangle, p_Color, p_Origin, p_Rotation, p_Scale, p_Orientation, p_Depth);
	}

	bool SpriteBatch::DrawSprite(const Texture& p_Texture, const Rectangle& p_DestinationRectangle, const Rectangle& p_SourceRectangle, const Color& p_Color, const Vector2& p_Origin, float p_Rotation, const Vector2& p_Scale, Orientation p_Orientation, float p_Depth)
	{
		// TODO: Factor in p_DestinationRectangle BETTERLY.
		return DrawSprite(p_Texture, Vector2(p_DestinationRectangle.X, p_DestinationRectangle.Y), p_SourceRectangle, p_Color, p_Origin, p_Rotation, p_Scale, p_Orientation, p_Depth);
	}

	bool SpriteBatch::DrawSprite(const Texture& p_Texture, const Vector2& p_Position, const Rectangle& p_SourceRectangle, const Color& p_Color, const Vector2& p_Origin, float p_Rotation, const Vector2& p_Scale, Orientation p_Orientation, float p_Depth)
	{
		if (!m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::DrawSprite] Must start a draw pair first.");
			return false;
		}

		float textureWidth = static_cast<float>(p_Texture.GetWidth());
		float textureHeight = static_cast<float>(p_Texture.GetHeight());

		float correctionX = 1.0f / textureWidth;
		float correctionY = 1.0f / textureHeight;

		Rectangle textureBounds;
		if (p_SourceRectangle != Rectangle::Empty)
			textureBounds = p_SourceRectangle;
		else
			textureBounds = Rectangle(0, 0, textureWidth, textureHeight);

		float sizeX = textureBounds.Width * p_Scale.X;
		float sizeY = textureBounds.Height * p_Scale.Y;

		float originX = -p_Origin.X * p_Scale.X;
		float originY = -p_Origin.Y * p_Scale.Y;

		float texCoordTop = textureBounds.Y / textureHeight + correctionY;
		float texCoordBottom = (textureBounds.Y + textureBounds.Height) / textureHeight - correctionY;
		float texCoordLeft = textureBounds.X / textureWidth + correctionX;
		float texCoordRight = (textureBounds.X + textureBounds.Width) / textureWidth - correctionX;

		// Calculate cos/sin for rotation in radians.
		float cosAngle = cosf(p_Rotation);
		float sinAngle = sinf(p_Rotation);

		// Flip texture coordinates for orientation.
		if ((p_Orientation & Orientation::FlipHorizontal) == Orientation::FlipHorizontal)
			std::swap(texCoordLeft, texCoordRight);
		if ((p_Orientation & Orientation::FlipVertical) == Orientation::FlipVertical)
			std::swap(texCoordTop, texCoordBottom);

		SpriteBatchItem& item = GetNextItem(p_Texture, p_Depth);

		// Top left vertex.
		SetVertexPosition(item.VertexTL, p_Position.X + originX * cosAngle - originY * sinAngle,
										 p_Position.Y + originX * sinAngle + originY * cosAngle);
		SetVertexColor(item.VertexTL, p_Color);
		SetVertexTexCoords(item.VertexTL, texCoordLeft, texCoordTop);

		// Top right vertex.
		SetVertexPosition(item.VertexTR, p_Position.X + (originX + sizeX) * cosAngle - originY * sinAngle,
										 p_Position.Y + (originX + sizeX) * sinAngle + originY * cosAngle);
		SetVertexColor(item.VertexTR, p_Color);
		SetVertexTexCoords(item.VertexTR, texCoordRight, texCoordTop);

		// Bottom left vertex.
		SetVertexPosition(item.VertexBL, p_Position.X + originX * cosAngle - (originY + sizeY) * sinAngle,
										 p_Position.Y + originX * sinAngle + (originY + sizeY) * cosAngle);
		SetVertexColor(item.VertexBL, p_Color);
		SetVertexTexCoords(item.VertexBL, texCoordLeft, texCoordBottom);

		// Bottom right vertex.
		SetVertexPosition(item.VertexBR, p_Position.X + (originX + sizeX) * cosAngle - (originY + sizeY) * sinAngle,
										 p_Position.Y + (originX + sizeX) * sinAngle + (originY + sizeY) * cosAngle);
		SetVertexColor(item.VertexBR, p_Color);
		SetVertexTexCoords(item.VertexBR, texCoordRight, texCoordBottom);

		return true;
	}

	bool SpriteBatch::DrawString(const SAGE::SpriteFont& p_SpriteFont, const std::string& p_String, const Vector2& p_Position, const Color& p_Color, float p_Depth)
	{
		return DrawString(p_SpriteFont, p_String, p_Position, p_Color, Vector2::Zero, 0.0f, Vector2::One, SAGE::Orientation::None, p_Depth);
	}

	bool SpriteBatch::DrawString(const SAGE::SpriteFont& p_SpriteFont, const std::string& p_String, const Vector2& p_Position, const Color& p_Color, const Vector2& p_Origin, float p_Rotation, const Vector2& p_Scale, Orientation p_Orientation, float p_Depth)
	{
		if (!m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::DrawString] Must start a draw pair first.");
			return false;
		}

		const Texture& texture = p_SpriteFont.GetTexture();

		int texWidth = texture.GetWidth();
		int texHeight = texture.GetHeight();

		float size = p_SpriteFont.GetSize();
		float spacing = p_SpriteFont.GetSpacing();

		float cosAngle = cosf(p_Rotation);
		float sinAngle = sinf(p_Rotation);

		Vector2 correction(1.0f / (float)texWidth, 1.0f / (float)texHeight);
		Vector2 origin(-p_Origin.X * p_Scale.X, -p_Origin.Y * p_Scale.Y);

		Vector2 offset = Vector2::Zero;

		for (char glyph : p_String)
		{
			if (glyph == '\n' || glyph == '\r')
			{
				offset.X = 0.0f;
				offset.Y += size * p_Scale.Y;
			}
			else
			{
				Rectangle rect = p_SpriteFont.GetGlyphBounds(glyph);

				Vector2 size(rect.Width * p_Scale.X, rect.Height * p_Scale.Y);
				Vector2 texCoordTL(rect.X / (float)texWidth + correction.X, rect.Y / (float)texHeight + correction.X);
				Vector2 texCoordBR((rect.X + rect.Width) / (float)texWidth - correction.X, (rect.Y + rect.Height) / (float)texHeight - correction.Y);

				SpriteBatchItem& item = GetNextItem(texture, p_Depth);

				// Top left vertex.
				SetVertexPosition(item.VertexTL, p_Position.X + origin.X + offset.X,
												 p_Position.Y + origin.Y + offset.Y);
				SetVertexColor(item.VertexTL, p_Color);
				SetVertexTexCoords(item.VertexTL, texCoordTL.X, texCoordTL.Y);

				// Top right vertex.
				SetVertexPosition(item.VertexTR, p_Position.X + origin.X + size.X + offset.X,
												 p_Position.Y + origin.Y + offset.Y);
				SetVertexColor(item.VertexTR, p_Color);
				SetVertexTexCoords(item.VertexTR, texCoordBR.X, texCoordTL.Y);

				// Bottom left vertex.
				SetVertexPosition(item.VertexBL, p_Position.X + origin.X + offset.X,
												 p_Position.Y + origin.Y + size.Y + offset.Y);
				SetVertexColor(item.VertexBL, p_Color);
				SetVertexTexCoords(item.VertexBL, texCoordTL.X, texCoordBR.Y);

				// Bottom right vertex.
				SetVertexPosition(item.VertexBR, p_Position.X + origin.X + size.X + offset.X,
												 p_Position.Y + origin.Y + size.Y + offset.Y);
				SetVertexColor(item.VertexBR, p_Color);
				SetVertexTexCoords(item.VertexBR, texCoordBR.X, texCoordBR.Y);

				TransformVerticesAbout(item, p_Position, cosAngle, sinAngle, p_Orientation);

				offset.X += spacing * p_Scale.X;
			}
		}

		return false;
	}

	bool SpriteBatch::End()
	{
		if (!m_WithinDrawPair)
		{
			SDL_Log("[SpriteBatch::End] Cannot end a pair without starting.");
			return false;
		}

		switch (m_SortMode)
		{
			case SortMode::Texture:
				std::sort(std::begin(m_BatchItemList), std::end(m_BatchItemList), [](const SpriteBatchItem& p_ItemA, const SpriteBatchItem& p_ItemB) { return p_ItemA.TextureID > p_ItemB.TextureID; });
				break;
			case SortMode::FrontToBack:
				std::sort(std::begin(m_BatchItemList), std::end(m_BatchItemList), [](const SpriteBatchItem& p_ItemA, const SpriteBatchItem& p_ItemB) { return p_ItemA.Depth > p_ItemB.Depth; });
				break;
			case SortMode::BackToFront:
				std::sort(std::begin(m_BatchItemList), std::end(m_BatchItemList), [](const SpriteBatchItem& p_ItemA, const SpriteBatchItem& p_ItemB) { return p_ItemA.Depth < p_ItemB.Depth; });
				break;
			case SortMode::Immediate:
			case SortMode::None:
				break;
		}

		switch (m_BlendMode)
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

		switch (m_RasterizerState)
		{
			case RasterizerState::None:
				glDisable(GL_CULL_FACE);
				break;
			case RasterizerState::CullClockwise:
				glEnable(GL_CULL_FACE);
				glCullFace(GL_FRONT);
				break;
			case RasterizerState::CullCounterClockwise:
				glEnable(GL_CULL_FACE);
				glCullFace(GL_BACK);
				break;
		}

		if (m_ItemCount > 0)
			Render();

		m_WithinDrawPair = false;

		return true;
	}

	void SpriteBatch::Render()
	{
		int length = 0;
		int texID = -1;

		for (int i = 0; i < m_ItemCount; ++i)
		{
			SpriteBatchItem& item = m_BatchItemList[i];

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

			m_VertexBuffer[length * 4 + 0] = item.VertexBL;
			m_VertexBuffer[length * 4 + 1] = item.VertexBR;
			m_VertexBuffer[length * 4 + 2] = item.VertexTR;
			m_VertexBuffer[length * 4 + 3] = item.VertexTL;

			length++;
		}

		Flush(texID, length);
	}

	void SpriteBatch::Flush(int p_TextureID, int p_Length)
	{
		// Ensure there's something to draw.
		if (p_Length == 0)
			return;

		// Enable textures and bind the current ID.
		glEnable(GL_TEXTURE_2D);
		glBindTexture(GL_TEXTURE_2D, p_TextureID);

		// Bind the vertex array.
		glBindVertexArray(m_VertexArrayObject);

		// Bind the vertex buffer.
		glBindBuffer(GL_ARRAY_BUFFER, m_VertexBufferObject);

		// Option 1: Insert subset into buffer.
		glBufferSubData(GL_ARRAY_BUFFER, 0, p_Length * 4 * sizeof(VertexPositionColorTexture), m_VertexBuffer);

		// Option 2: Write to buffer mapping.
		/*
		glBufferData(GL_ARRAY_BUFFER, m_VertexBuffer.size() * sizeof(VertexPositionColorTexture), nullptr, GL_DYNAMIC_DRAW);
		VertexPositionColorTexture* map = reinterpret_cast<VertexPositionColorTexture*>(
		glMapBufferRange(GL_ARRAY_BUFFER,
		0,
		p_Length * sizeof(VertexPositionColorTexture),
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT)
		);
		std::copy(m_VertexBuffer.begin(), m_VertexBuffer.end(), map);
		glUnmapBuffer(GL_ARRAY_BUFFER);
		*/

		// Bind the element buffer and draw.
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IndexBufferObject);
		glDrawElements(GL_TRIANGLES, p_Length * 6, GL_UNSIGNED_SHORT, nullptr);

		// Clear bindings.
		glBindVertexArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

		// Disable textures.
		glDisable(GL_TEXTURE_2D);

		m_FlushCount++;
	}

	SpriteBatchItem& SpriteBatch::GetNextItem(const Texture& p_Texture, float p_Depth)
	{
		SpriteBatchItem& item = m_BatchItemList[m_ItemCount++];
		item.TextureID = p_Texture.GetID();
		item.Depth = p_Depth;

		return item;
	}

	void SpriteBatch::SetVertexPosition(VertexPositionColorTexture& p_Vertex, float p_X, float p_Y)
	{
		p_Vertex.Position.X = p_X;
		p_Vertex.Position.Y = p_Y;
	}

	void SpriteBatch::SetVertexColor(VertexPositionColorTexture& p_Vertex, const Color& p_Color)
	{
		p_Vertex.Color.R = p_Color.GetRed();
		p_Vertex.Color.G = p_Color.GetGreen();
		p_Vertex.Color.B = p_Color.GetBlue();
		p_Vertex.Color.A = p_Color.GetAlpha();
	}

	void SpriteBatch::SetVertexTexCoords(VertexPositionColorTexture& p_Vertex, float p_U, float p_V)
	{
		p_Vertex.TexCoord.X = p_U;
		p_Vertex.TexCoord.Y = p_V;
	}

	void SpriteBatch::TransformVerticesAbout(SpriteBatchItem& p_Item, const Vector2& p_Position, float p_CosAngle, float p_SinAngle, Orientation p_Orientation)
	{
		RotateVerticesAbout(p_Item, p_Position, p_CosAngle, p_SinAngle);
		FlipVerticesAbout(p_Item, p_Position, p_Orientation);
	}

	void SpriteBatch::RotateVerticesAbout(SpriteBatchItem& p_Item, const Vector2& p_Position, float p_CosAngle, float p_SinAngle)
	{
		RotateVertexAbout(p_Item.VertexTL, p_Position, p_CosAngle, p_SinAngle);
		RotateVertexAbout(p_Item.VertexTR, p_Position, p_CosAngle, p_SinAngle);
		RotateVertexAbout(p_Item.VertexBL, p_Position, p_CosAngle, p_SinAngle);
		RotateVertexAbout(p_Item.VertexBR, p_Position, p_CosAngle, p_SinAngle);
	}

	void SpriteBatch::FlipVerticesAbout(SpriteBatchItem& p_Item, const Vector2& p_Position, Orientation p_Orientation)
	{
		FlipVertexAbout(p_Item.VertexTL, p_Position, p_Orientation);
		FlipVertexAbout(p_Item.VertexTR, p_Position, p_Orientation);
		FlipVertexAbout(p_Item.VertexBL, p_Position, p_Orientation);
		FlipVertexAbout(p_Item.VertexBR, p_Position, p_Orientation);
	}

	void SpriteBatch::RotateVertexAbout(VertexPositionColorTexture& p_Vertex, const Vector2& p_Position, float p_CosAngle, float p_SinAngle)
	{
		// Shift to transform origin.
		p_Vertex.Position.X -= p_Position.X;
		p_Vertex.Position.Y -= p_Position.Y;

		// Rotate the vertex about the new origin.
		float rotationX = p_Vertex.Position.X * p_CosAngle - p_Vertex.Position.Y * p_SinAngle;
		float rotationY = p_Vertex.Position.X * p_SinAngle + p_Vertex.Position.Y * p_CosAngle;

		// Shift back to the original origin.
		p_Vertex.Position.X = rotationX + p_Position.X;
		p_Vertex.Position.Y = rotationY + p_Position.Y;
	}

	void SpriteBatch::FlipVertexAbout(VertexPositionColorTexture& p_Vertex, const Vector2& p_Position, Orientation p_Orientation)
	{
		// Flip texture vertices horizontally.
		if ((p_Orientation & Orientation::FlipHorizontal) == Orientation::FlipHorizontal)
		{
			p_Vertex.Position.X = p_Position.X - (p_Vertex.Position.X - p_Position.X);
		}

		// Flip texture vertices vertically.
		if ((p_Orientation & Orientation::FlipVertical) == Orientation::FlipVertical)
		{
			p_Vertex.Position.Y = p_Position.Y - (p_Vertex.Position.Y - p_Position.Y);
		}
	}
}
