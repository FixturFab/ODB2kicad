#pragma once
#include "../string.h"
#include "../stream.h"

class wxXmlAttribute {
public:
    wxXmlAttribute() = default;
    wxXmlAttribute(const wxString& name, const wxString& value, wxXmlAttribute* next = nullptr)
        : m_name(name), m_strValue(value), m_next(next) {}
    virtual ~wxXmlAttribute() { delete m_next; }

    wxString GetName() const { return m_name; }

    // String-based GetValue/SetValue for base class
    wxString GetValue() const { return m_strValue; }
    void SetValue(const wxString& value) { m_strValue = value; }

    wxXmlAttribute* GetNext() const { return m_next; }
    void SetNext(wxXmlAttribute* next) { m_next = next; }
    void SetName(const wxString& name) { m_name = name; }

protected:
    wxString m_name;
    wxString m_strValue;
    wxXmlAttribute* m_next = nullptr;
};

// Global enum for wxXmlNodeType (used outside wxXmlNode class scope)
enum wxXmlNodeType { wxXML_ELEMENT_NODE = 1, wxXML_ATTRIBUTE_NODE, wxXML_TEXT_NODE,
                     wxXML_CDATA_SECTION_NODE, wxXML_COMMENT_NODE, wxXML_DOCUMENT_NODE };

class wxXmlNode {
public:
    typedef ::wxXmlNodeType wxXmlNodeType;

    wxXmlNode() = default;
    wxXmlNode(wxXmlNodeType type, const wxString& name, const wxString& content = wxString())
        : m_type(type), m_name(name), m_content(content) {}
    wxXmlNode(wxXmlNode* parent, wxXmlNodeType type, const wxString& name,
              const wxString& content = wxString(), wxXmlAttribute* props = nullptr)
        : m_type(type), m_name(name), m_content(content), m_parent(parent), m_attrs(props) {
        if(parent) parent->AddChild(this);
    }
    virtual ~wxXmlNode() { delete m_children; delete m_next; delete m_attrs; }

    wxXmlNodeType GetType() const { return m_type; }
    wxString GetName() const { return m_name; }
    wxString GetContent() const { return m_content; }
    wxString GetNodeContent() const { return m_content; }
    wxXmlNode* GetChildren() const { return m_children; }
    wxXmlNode* GetNext() const { return m_next; }
    wxXmlNode* GetParent() const { return m_parent; }
    wxXmlAttribute* GetAttributes() const { return m_attrs; }

    bool GetAttribute(const wxString& name, wxString* value) const {
        for(auto* a = m_attrs; a; a = a->GetNext()) {
            if(a->GetName() == name) { if(value) *value = a->GetValue(); return true; }
        }
        return false;
    }
    wxString GetAttribute(const wxString& name, const wxString& defaultVal = wxString()) const {
        wxString v;
        return GetAttribute(name, &v) ? v : defaultVal;
    }
    bool HasAttribute(const wxString& name) const { return GetAttribute(name, (wxString*)nullptr); }

    virtual void AddAttribute(const wxString& name, const wxString& value) {
        auto* attr = new wxXmlAttribute(name, value, m_attrs);
        m_attrs = attr;
    }
    virtual void AddAttribute(wxXmlAttribute* attr) {
        if(attr) { attr->SetNext(m_attrs); m_attrs = attr; }
    }
    void AddChild(wxXmlNode* child) {
        if(!m_children) { m_children = child; }
        else {
            wxXmlNode* last = m_children;
            while(last->m_next) last = last->m_next;
            last->m_next = child;
        }
        child->m_parent = this;
    }
    void SetContent(const wxString& content) { m_content = content; }
    void SetName(const wxString& name) { m_name = name; }
    void SetNext(wxXmlNode* next) { m_next = next; }

protected:
    wxXmlNodeType m_type = wxXML_ELEMENT_NODE;
    wxString m_name, m_content;
    wxXmlNode* m_children = nullptr;
    wxXmlNode* m_next = nullptr;
    wxXmlNode* m_parent = nullptr;
    wxXmlAttribute* m_attrs = nullptr;
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
