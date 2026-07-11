/**
 * @file TextTests.cpp
 * @brief text 模块测试。
 */

#include "../TestSupport.hpp"
#include "../ZipFixture.hpp"
#include "cc/inventory/InventoryEngine.hpp"
#include "cc/loader/ProjectLoader.hpp"
#include "cc/text/OpenXmlTextExtractor.hpp"
#include "cc/text/PdfTextExtractor.hpp"
#include "cc/text/StructuredTextExtractor.hpp"
#include "cc/text/TextExtractionService.hpp"

#include <fstream>

void runTextTests() {
    auto context = cc::ProjectLoader{}.load(sourceDir() / "examples/business_bad_case");
    auto inventory = cc::InventoryEngine{}.build(context.value());
    auto corpus = cc::TextExtractionService{}.extract(inventory.value());
    requireTrue(corpus.ok(), "text extraction should succeed");
    requireTrue(!corpus.value().empty(), "text corpus should not be empty");

    const auto docx = sourceDir() / "build/text_fixture.docx";
    std::filesystem::remove(docx);
    contest_test::writeStoredZipFixture(
        docx, {{"word/document.xml", "<w:document><w:body><w:p><w:r><w:t>OpenXML "
                                     "项目文本</w:t></w:r></w:p></w:body></w:document>"}});

    cc::ProjectAsset openXmlAsset;
    openXmlAsset.absolutePath = docx;
    openXmlAsset.relativePath = "text_fixture.docx";
    openXmlAsset.fileName = "text_fixture.docx";
    openXmlAsset.extension = ".docx";
    auto openXml = cc::OpenXmlTextExtractor{}.extract(openXmlAsset);
    requireTrue(openXml.ok(), "OpenXML extraction should return result");
    requireTrue(openXml.value().status == "EXTRACTED_OPENXML", "OpenXML should be extracted");
    requireTrue(openXml.value().text.find("OpenXML") != std::string::npos, "OpenXML text missing");

    const auto pdfPath = sourceDir() / "build/text_fixture.pdf";
    {
        std::ofstream pdf(pdfPath, std::ios::binary);
        const std::string stream = "BT /F1 12 Tf 72 720 Td (PDF 项目文本) Tj ET";
        pdf << "%PDF-1.4\n"
            << "1 0 obj\n<< /Type /Catalog >>\nendobj\n"
            << "2 0 obj\n<< /Length " << stream.size() << " >>\nstream\n"
            << stream << "\nendstream\nendobj\n%%EOF\n";
    }
    cc::ProjectAsset realPdfAsset;
    realPdfAsset.absolutePath = pdfPath;
    realPdfAsset.relativePath = "text_fixture.pdf";
    realPdfAsset.fileName = "text_fixture.pdf";
    realPdfAsset.extension = ".pdf";
    auto realPdf = cc::PdfTextExtractor{}.extract(realPdfAsset);
    requireTrue(realPdf.ok(), "PDF extractor should parse simple content stream");
    requireTrue(realPdf.value().status == "EXTRACTED_PDF", "PDF text should be extracted");
    requireTrue(realPdf.value().text.find("PDF 项目文本") != std::string::npos,
                "PDF text content missing");

    cc::ProjectAsset pdfAsset;
    pdfAsset.absolutePath = sourceDir() / "examples/business_bad_case/商业计划书.md";
    pdfAsset.relativePath = "fake.pdf";
    pdfAsset.fileName = "fake.pdf";
    pdfAsset.extension = ".pdf";
    auto pdf = cc::PdfTextExtractor{}.extract(pdfAsset);
    requireTrue(pdf.ok(), "PDF extractor should return result even when review is needed");
    requireTrue(pdf.value().status == "NEED_REVIEW_PDF_TEXT_EXTRACTION_LIMITED",
                "invalid PDF should be marked for review");

    const auto jsonPath = sourceDir() / "build/text_fixture.json";
    {
        std::ofstream json(jsonPath);
        json << "{\"project\":{\"name\":\"可信编译器\",\"target_user\":\"学生\"}}";
    }
    cc::ProjectAsset jsonAsset;
    jsonAsset.absolutePath = jsonPath;
    jsonAsset.relativePath = "text_fixture.json";
    jsonAsset.fileName = "text_fixture.json";
    jsonAsset.extension = ".json";
    auto structuredJson = cc::StructuredTextExtractor{}.extract(jsonAsset);
    requireTrue(structuredJson.ok(), "structured json extraction should succeed");
    requireTrue(structuredJson.value().text.find("key: project.name") != std::string::npos,
                "json keys should be extracted");
    requireTrue(structuredJson.value().text.find("可信编译器") != std::string::npos,
                "json string values should be extracted");

    const auto yamlPath = sourceDir() / "build/text_fixture.yaml";
    {
        std::ofstream yaml(yamlPath);
        yaml << "target_user: 学生\nsolution: 可信审计\n";
    }
    cc::ProjectAsset yamlAsset;
    yamlAsset.absolutePath = yamlPath;
    yamlAsset.relativePath = "text_fixture.yaml";
    yamlAsset.fileName = "text_fixture.yaml";
    yamlAsset.extension = ".yaml";
    auto structuredYaml = cc::StructuredTextExtractor{}.extract(yamlAsset);
    requireTrue(structuredYaml.ok(), "structured yaml extraction should succeed");
    requireTrue(structuredYaml.value().text.find("key: target_user") != std::string::npos,
                "yaml keys should be extracted");
    requireTrue(structuredYaml.value().text.find("可信审计") != std::string::npos,
                "yaml values should be extracted");
}
