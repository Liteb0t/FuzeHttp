<h1><img src="https://fuze.page/static/fuze-min-hover.png" style="max-width: 100%;" width="94" height="45" alt="FUZE"> Http</h1>
<p>
FuzeHttp<sup>[<a href="https://github.com/Liteb0t/FuzeHttp">github</a>]</sup> is a web framework written in C++23, designed for modern REST API-based services. It is gradually being developed in an ad-hoc way, to meet the growing needs of <a href="https://fuze.page/software/mediaboard">Fuze Mediaboard</a>.
</p>
<h2>Building the example project</h2>
The easiest way to get started is to set up the example project, and then work from there. If you have deployed Fuze Mediaboard before, these steps will be very familiar.
<h3>Compiling from source</h3>
<p>
	Prerequisites: Git, CMake (>= 3.28), Boost (>= 1.88), SQLite OR PostgreSQL.<br><br>
	First ensure that submodules are downloaded. Use this command:<br>
	<code>git submodule update --init --recursive</code><br>
	Now, to compile the server:<br>
	<code>cmake -B build -G Ninja -D WITH_EXAMPLE=ON</code><br>
	<code>cmake --build build</code><br>
	<i>Compiling is known to work with clang-19, but not GCC 14.</i>
</p>
<h3>Running the example project</h3>
<p>
	This is the same procedure to create the owner account, as in Fuze Mediaboard.<br>
	If compiled from source, execute <code>./build/bin/example --create_owner</code>
	The <code>--create_owner</code> flag is required on first boot. When the server starts, an invitation link will appear in the command-line output.
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_register_owner_account_link.png" alt="Registration link">
		<figcaption>Invitation link to create owner account.</figcaption>
	</figure>
	<figure>
		<img src="https://fuze.page/static/Fuze_Mediaboard_0.1.3_accept_invitation.png" alt="Accepting the invitation">
		<figcaption>A registration form is shown when invitation is opened</figcaption>
	</figure>
	After registering the owner account, you can now interact with the server.<br>
	<i>If you ever forget the password, you can simply run the create_owner command again. Note that it will not be the same account.</i>
</p>
<h2>Program options</h2>
<p>
Options can be defined in <code>config.ini</code> or passed in at runtime. Run the server with <code>--help</code> to see all available options. Some options are built-in, such as <code>threads</code> and <code>environment_variable_for_secret</code>.<br><br>
Additional options can be defined. The container for additional options is instantiated in this way:<br>
<code>FuzeHttp::ProgramOptions options;</code><br>
We can see the three option types inherited from FuzeHttp::ProgramOptionBase in the example project's addProgramOptions function:
<pre><code>
options->addOptions()
	(new ProgramOption&lt;std::string>("favicon_url", "https://fuze.page/favicon.ico"))
	(new ProgramConstant("test_program_constant", 73))
	(new ProgramOptionPtr("site_name", &state_config->server_name, {.default_value=std::string("FuzeHttp Example")}));
</code></pre>
These classes accept a template argument <code>OptionType</code>, which can be any type with an <code>operator>></code> overload. Here is a description of the three option types:
<table>
	<tr><th>class</th><th>description</th></tr>
	<tr><td><code>
				template&lt;typename OptionType><br>ProgramOption( std::string token, OptionType default_value, std::string description = "")</code></td><td>The default_value is stored within this object and can be overridden by <code>config.ini</code> or command-line arguments.</td></tr>
	<tr><td><code>template&lt;typename OptionType><br>ProgramConstant(std::string token, OptionType default_value, std::string description = "")</code></td><td>Same as <code>ProgramOption</code> except the value is absolute - it cannot be overriden. It also does not appear in the --help command.</td></tr>
	<tr><td><code>template&lt;typename OptionType><br>ProgramOptionPtr(std::string token, OptionType* value_ptr, Args args = {})</code><br>
			<code>struct Args { std::optional&lt;OptionType> default_value; const char* description = ""; bool include_in_frontend = true; }</code></td><td>Same as <code>ProgramOption</code> except the value is stored elsewhere, and this object holds a raw pointer to that value.</td></tr>
</table>
</p>
<h2>Frontend</h2>
<p>
Unlike <abbr title="Model-View-Controller">MVC</abbr> frameworks, FuzeHttp does not provide live <abbr title="Server-side rendering">SSR</abbr>.
</p>
<h3>File inclusion</h3>
<p>
Relative paths to files should be prepended with <code>FILE_</code>, so that cache control will work properly. Otherwise, the client can load files from different versions, leading to unreproducible errors. For example:<br>
<code>&lt;link rel="stylesheet" href=FILE_"static/styles.css"></code><br>
This should not be done for external resources, only internal assets contained in the frontend folder.
</p>
<h3>Using program options</h3>
<p>
<code>ProgramOption</code>, <code>ProgramOptionPtr</code>, and <code>ProgramConstant</code> entries can be included in the frontend, by prepending <code>CONFIG_</code> to the key. On startup, the server will fill in the values.<br>
Given the example project has this option entry:
<pre><code>(new ProgramConstant("test_program_constant", 73))
</code></pre>
In the frontend, <code>CONFIG_test_program_constant</code> is replaced with <code>73</code>.
</p>
<h2>Controller</h2>
<p>
All HTTP requests are routed through the controller. A pattern can be added to the controller, which matches a request URL to a view.
</p>
<h3>Pattern</h3>
<p>URLs can be added to the controller with the following function:<br>
<code>template&lt;typename... Types>
	void addPattern(http::verb req_method, typename MakeFuncPtr&lt;StateType, typename GetHandlerArgs&lt;TypeList&lt;Types...>, ToHandlerArg>::type>::type view, Types... args)</code><br>
A pattern consists of a method, a callback, an optional Client parameter, and a set of path segments. Path segments can be <code>const char*</code>, <code>int{}</code>, <code>std::string{}</code>, or a <code>Resolver</code>.
</p>
<h3>Resolver</h3>
<p>
A resolver is a user-specified overload of <code>ResolverBase</code>. It can pass an object or pointer into the view, or return a user-defined Response, typically with an error status when the object is not found or authorization failed.
<details><summary>Example</summary>
<h4>resolvers.cppm - Fuze Mediaboard</h4>
<pre><code>template&lt;PERMISSION permission = PERMISSION::NUMBER_OF_PERMISSIONS> // NUMBER_OF_PERMISSIONS means "none"
struct BoardResolver : Resolver&lt;State*, Board*, std::string> {
	virtual std::expected&lt;void, Response> validateExtraPermission(Board* board, const std::optional&lt;Client>& client) const {return {};}
	std::expected&lt;std::any, FuzeHttp::Response> fetch(State* state, std::string key, const std::optional&lt;Client>& client) const override {
		auto board_res = state->getBoardIfExists(key);
		if (!board_res)
			return std::unexpected(Response{.status=http::status::not_found, .error_message=std::format("Could not find board with slug {}", key)});
		auto board = board_res.value();
		if (!board->clientHasPermission(client, static_cast&lt;int>(PERMISSION::VIEW_BOARD))) {
			return std::unexpected(Response{.status=http::status::forbidden, .error_message=std::format("Client does not have permission to access board `{}`", key)});
		}
		if (auto permission_res = validateExtraPermission(board, client); !permission_res)
			return std::unexpected(permission_res.error());
		return board;
	}
};

template&lt;PERMISSION permission>
requires (permission != PERMISSION::NUMBER_OF_PERMISSIONS)
struct BoardResolver&lt;permission> : BoardResolver&lt;PERMISSION::NUMBER_OF_PERMISSIONS> {
	virtual std::expected&lt;void, Response> validateExtraPermission(Board* board, const std::optional&lt;Client>& client) const override {
		if (!board->clientHasPermission(client, static_cast&lt;int>(permission)))
			return std::unexpected(Response{.status=http::status::forbidden, .error_message=std::format("Client does not have permission to perform this action on board `{}`", board->getSlug())});
		return {}; // success
	}
};
</pre></code>
<h4>urls.cppm - Fuze Mediaboard</h4>
<pre><code>	(verb::get, getBoard, 							"api", "board", BoardResolver{})
	(verb::put, editBoard, Client{},				"api", "board", BoardResolver&lt;PERMISSION::CREATE_BOARD>{})
	(verb::delete_, deleteBoard, Client{},			"api", "board", BoardResolver&lt;PERMISSION::DELETE_BOARD>{})
</code></pre>
<h4>views.cppm - Fuze Mediaboard</h4>
<pre><code>FuzeHttp::Response deleteBoard(Mediaboard::State* state, FuzeHttp::Request req, Client client, Board* board) {
	// permission to delete board has been checked by the Resolver
	board->markAsDeleted();
	return Response{.status = http::status::no_content};
}
</pre></code>
</details>
A Resolver can also take a parent object, by passing a fourth template argument to <code>ResolverBase</code>
<details><summary>Example</summary>
<h4>resolvers.cppm - Fuze Mediaboard</h4>
<code><pre>template&lt;PERMISSION permission = PERMISSION::NUMBER_OF_PERMISSIONS>
struct ThreadResolver : Resolver&lt;State*, Thread*, int, Board*> {
	virtual std::expected&lt;void, Response> validateExtraPermission(Thread* thread, const std::optional&lt;Client>& client) const {return {};}
	std::expected&lt;std::any, FuzeHttp::Response> fetch(State* state, int thread_id, Board* board, const std::optional&lt;Client>& client) const override {
		if (!board->threadExists(thread_id))
			return std::unexpected(Response{.status = http::status::not_found, .error_message = std::format("Thread {} was not found.", thread_id)});
		Thread* thread = board->getThread(thread_id);
		// ...
</code></pre>
<h4>urls.cppm - Fuze Mediaboard</h4>
<code><pre>	// A resolver which takes a parent object must have the parent existing as an earlier segment
	// ie, ThreadResolver relies on the value returned from BoardResolver
	(verb::get, getThread, 							"api", "board", BoardResolver{}, "thread", ThreadResolver{})
</code></pre>
</details>
</p>
<h3>View</h3>
<p>
This is a callback function which is called when its corresponding pattern in the controller matches the request URL. Every view contains arguments for the state and the request, plus variable path segments.
</p>
<h3>Response</h3>
<p>
Every view must return a <code>FuzeHttp::Response</code>. The <code>status</code> is required.
<pre><code>struct Response {
	beast::http::status status;
	std::unordered_map&lt;std::string, std::string> headers;
	std::optional&lt;std::string> error_message;
	std::optional&lt;boost::json::value> json;
	std::optional&lt;std::filesystem::path> file;
	std::optional&lt;std::string> body;
};
</code></pre>
</p>
