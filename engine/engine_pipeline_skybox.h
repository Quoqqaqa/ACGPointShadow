#pragma once

class ENG_API PipelineSkybox final : public Eng::Pipeline {
public:
	PipelineSkybox();
	PipelineSkybox(PipelineSkybox &&other);
	PipelineSkybox(PipelineSkybox const&) = delete;
	~PipelineSkybox();

	bool render(const Eng::Texture& texture, const Eng::List& list);

	bool init() override;
	bool free() override;

private:
	struct Reserved;
	std::unique_ptr<Reserved> reserved;

	PipelineSkybox(const std::string& name);
};