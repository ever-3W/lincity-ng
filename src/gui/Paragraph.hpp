#ifndef __TEXTBOX_HPP__
#define __TEXTBOX_HPP__

#include <vector>
#include <string>
#include "Texture.hpp"
#include "Component.hpp"
#include "Style.hpp"
#include "DocumentElement.hpp"
#include "callback/Signal.hpp"

class XmlReader;

/**
 * contains a part of text in normalized form (all spaces collapsed to ' ')
 */
class TextSpan
{
public:
    Style style;
    std::string text;
};

/**
 * A paragraph of (formatted) text
 * The text is constructed from a list of TextSpans. A TextSpan combines a list
 * of words with a Style so that it can have different font attributes and
 * color.
 */
class Paragraph : public Component, public DocumentElement
{
public:
    Paragraph();
    virtual ~Paragraph();

    void parse(XmlReader& reader);
    // hack to parse <li> elements for now...
    void parseList(XmlReader& reader, const Style& parentstyle);
    void parse(XmlReader& reader, const Style& parentstyle);

    void resize(float width, float height);
    void draw(Painter& painter);
    void event(const Event& event);

    /**
     * sets a new text in the Paragraph. The style of the paragraph is used
     */
    void setText(const std::string& text);
    /**
     * Sets a new text in the Paragraph.
     */
    void setText(const std::string& text, const Style& style);
    
    std::string getText() const;

    const Style& getStyle() const
    {
        return style;
    }

    Signal<Paragraph*, const std::string&> linkClicked;

private:
    class LinkRectangle
    {
    public:
        Rect2D rect;
        const TextSpan* span;
    };
    
    typedef std::vector<TextSpan*> TextSpans;
    TextSpans textspans;
    Style style;

    Texture* texture;

    typedef std::vector<LinkRectangle> LinkRectangles;
    LinkRectangles linkrectangles;
};

#endif

