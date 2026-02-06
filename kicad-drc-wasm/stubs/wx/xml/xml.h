#pragma once
#include "../string.h"
#include "../stream.h"

class wxXmlAttribute {
public:
    wxXmlAttribute() = default;
    wxXmlAttribute(const wxString& name, const wxString& value) : m_name(name), m_value(value) {}
    wxString GetName() const { return m_name; }
    wxString GetValue() const { return m_value; }
    wxXmlAttribute* GetNext() const { return nullptr; }
private:
    wxString m_name, m_value;
};

class wxXmlNode {
public:
    enum wxXmlNodeType { wxXML_ELEMENT_NODE = 1, wxXML_ATTRIBUTE_NODE, wxXML_TEXT_NODE,
                         wxXML_CDATA_SECTION_NODE, wxXML_COMMENT_NODE, wxXML_DOCUMENT_NODE };

    wxXmlNode() = default;
    wxXmlNode(wxXmlNodeType type, const wxString& name, const wxString& content = wxString())
        : m_type(type), m_name(name), m_content(content) {}
    ~wxXmlNode() { delete m_children; delete m_next; }

    wxXmlNodeType GetType() const { return m_type; }
    wxString GetName() const { return m_name; }
    wxString GetContent() const { return m_content; }
    wxString GetNodeContent() const { return m_content; }
    wxXmlNode* GetChildren() const { return m_children; }
    wxXmlNode* GetNext() const { return m_next; }
    wxXmlNode* GetParent() const { return nullptr; }
    wxXmlAttribute* GetAttributes() const { return nullptr; }

    bool GetAttribute(const wxString& name, wxString* value) const { return false; }
    wxString GetAttribute(const wxString& name, const wxString& defaultVal = wxString()) const { return defaultVal; }
    bool HasAttribute(const wxString& name) const { return false; }

    void AddAttribute(const wxString& name, const wxString& value) {}
    void AddChild(wxXmlNode* child) { child->m_next = m_children; m_children = child; }
    void SetContent(const wxString& content) { m_content = content; }
    void SetName(const wxString& name) { m_name = name; }

private:
    wxXmlNodeType m_type = wxXML_ELEMENT_NODE;
    wxString m_name, m_content;
    wxXmlNode* m_children = nullptr;
    wxXmlNode* m_next = nullptr;
};

class wxXmlDocument {
public:
    wxXmlDocument() = default;
    wxXmlDocument(const wxString& filename) {}
    bool Load(const wxString& filename) { return false; }
    bool Load(wxInputStream& stream) { return false; }
    bool Save(const wxString& filename) const { return false; }
    bool Save(wxOutputStream& stream) const { return false; }
    bool IsOk() const { return false; }
    wxXmlNode* GetRoot() const { return nullptr; }
    void SetRoot(wxXmlNode*) {}
    wxString GetVersion() const { return wxString("1.0"); }
};
